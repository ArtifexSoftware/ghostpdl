/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcpage.c - PCL5 page and transformation control commands */

#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcparam.h"
#include "pcparse.h"		/* for pcl_execute_macro */
#include "pcfont.h"		/* for underline interface */
#include "pcpatxfm.h"
#include "pcursor.h"
#include "pcpage.h"
#include "pgmand.h"
#include "pginit.h"
#include "gsmatrix.h"		/* for gsdevice.h */
#include "gscoord.h"
#include "gsdevice.h"
#include "gspaint.h"
#include "gxdevice.h"
#include "gdevbbox.h"

/*
 * The PCL printable region. HP always sets the boundary of this region to be
 * 1/6" in from the edge of the paper, irrespecitve of the paper size or the
 * device. Since this dimension affects some default values related to rasters,
 * it is important that it be assigned this value.
 */
#define PRINTABLE_MARGIN_CP inch2coord(1.0 / 6.0)


/* Procedures */

/*
 * Preserve the current point and text margin set by transfroming them into
 * logical page space.
 */
  private void
preserve_cap_and_margins(
    const pcl_state_t *  pcs,
    gs_point *           pcur_pt,
    gs_rect *            ptext_rect
)
{
    pcur_pt->x = (double)pcl_cap.x;
    pcur_pt->y = (double)pcl_cap.y;
    gs_point_transform( pcur_pt->x,
                        pcur_pt->y,
                        &(pcs->xfm_state.pd2lp_mtx),
                        pcur_pt
                        );
    ptext_rect->p.x = (double)pcs->margins.left;
    ptext_rect->p.y = (double)pcs->margins.top;
    ptext_rect->q.x = (double)pcs->margins.right;
    ptext_rect->q.y = (double)(pcs->margins.top + pcs->margins.length);
    pcl_transform_rect(ptext_rect, ptext_rect, &(pcs->xfm_state.pd2lp_mtx));
}

/*
 * Convert the current point and text margin set back into "pseudo print
 * direction" space.
 */
  private void
restore_cap_and_margins(
    pcl_state_t *    pcs,
    const gs_point * pcur_pt,
    const gs_rect *  ptext_rect
)
{
    gs_matrix        lp2pd;
    gs_point         tmp_pt;
    gs_rect          tmp_rect;

    pcl_invert_mtx(&(pcs->xfm_state.pd2lp_mtx), &lp2pd);
    gs_point_transform(pcur_pt->x, pcur_pt->y, &lp2pd, &tmp_pt);
    pcl_cap.x = (coord)tmp_pt.x;
    pcl_cap.y = (coord)tmp_pt.y;
    pcl_transform_rect(ptext_rect, &tmp_rect, &lp2pd);
    pcs->margins.left = (coord)tmp_rect.p.x;
    pcs->margins.top = (coord)tmp_rect.p.y;
    pcs->margins.right = (coord)tmp_rect.q.x;
    pcs->margins.length = (coord)tmp_rect.q.y - pcs->margins.top;
}

/*
 * Update the transformations stored in the PCL state. This will also update
 * the device clipping region information in device and logical page space.
 * The text region margins are preserved.
 *
 * This routine should be called for:
 *
 *     changes in the paper size
 *     transition from page front to page back for duplexing
 *         (this facility is not currently implemented)
 *     change of left or top offset registration
 *     change of logical page orientation
 *     change of print direction
 *
 * The paper size, left/top offsets, logical page orientation, and print
 * direction should be set before this procedure is called.
 */
  private void
update_xfm_state(
    pcl_state_t *               pcs
)
{
    pcl_xfm_state_t *           pxfmst = &(pcs->xfm_state);
    const pcl_paper_size_t *    psize = pxfmst->paper_size;
    coord                       offset;
    gs_matrix                   pg2dev, pg2lp;
    gs_rect                     print_rect, dev_rect, text_rect;
    gs_point                    cur_pt;
    floatp                      loff = pxfmst->left_offset_cp;
    floatp                      toff = pxfmst->top_offset_cp;

    /* preserve the current point and text rectangle in logical page space */
    preserve_cap_and_margins(pcs, &cur_pt, &text_rect);

    /* get the page to device transformation */
    gs_defaultmatrix(pcs->pgs, &pg2dev);

    /*
     * Get the logical to page space transformation, and the dimensions of the
     * logical page.
     *
     * NOT YET IMPLEMENT - if on back of a duplex page, change size of offsets
     *
     * if (duplex_back(pcs)) {
     *    loff = -loff;
     *    toff = -toff;
     * }
     */
    pcl_make_rotation( pxfmst->lp_orient,
                       (floatp)(psize->width),
                       (floatp)(psize->height),
                       &(pxfmst->lp2pg_mtx)
                       );
    pxfmst->lp2pg_mtx.tx += loff;
    pxfmst->lp2pg_mtx.ty += toff;
    offset = ( (pxfmst->lp_orient & 0x1) != 0 ? psize->offset_landscape
	       : psize->offset_portrait );
    /* we need an extra 1/10 inch on each side to support 80
       characters vs. 78 at 10 cpi.  HP applies the change to Letter
       and A4.  We apply it to all paper sizes */
    if ( pcs->wide_a4 )
	offset -= inch2coord(1.0/10.0);

    gs_matrix_translate( &(pxfmst->lp2pg_mtx),
                         (floatp)offset,
                         0.0,
                         &(pxfmst->lp2pg_mtx)
                         );
    if ((pxfmst->lp_orient & 0x1) != 0) {
        pxfmst->lp_size.x = psize->height - 2 * offset;
        pxfmst->lp_size.y = psize->width;
    } else {
        pxfmst->lp_size.x = psize->width - 2 * offset;
        pxfmst->lp_size.y = psize->height;
    }

    /* then the logical page to device transformation */
    gs_matrix_multiply(&(pxfmst->lp2pg_mtx), &pg2dev,  &(pxfmst->lp2dev_mtx));

    /* the "pseudo page direction to logical page/device transformations */
    pcl_make_rotation( pxfmst->print_dir,
                       (floatp)pxfmst->lp_size.x,
                       (floatp)pxfmst->lp_size.y,
                       &(pxfmst->pd2lp_mtx)
                       );
    gs_matrix_multiply( &(pxfmst->pd2lp_mtx),
                        &(pxfmst->lp2dev_mtx),
               !        &(pxfmst->pd2dev_mtx)
                        );

    /* calculate the print direction page size */
    if ((pxfmst->print_dir) & 0x1) {
        pxfmst->pd_size.x = pxfmst->lp_size.y;
        pxfmst->pd_size.y = pxfmst->lp_size.x;
    } else
        pxfmst->pd_size = pxfmst->lp_size;

    /* calculate the device space clipping window */
    print_rect.p.x = PRINTABLE_MARGIN_CP;
    print_rect.p.y = PRINTABLE_MARGIN_CP;
    print_rect.q.x = psize->width - PRINTABLE_MARGIN_CP;
    print_rect.q.y = psize->height - PRINTABLE_MARGIN_CP;
    pcl_transform_rect(&print_rect, &dev_rect, &pg2dev);
    pxfmst->dev_print_rect.p.x = float2fixed(dev_rect.p.x);
    pxfmst->dev_print_rect.p.y = float2fixed(dev_rect.p.y);
    pxfmst->dev_print_rect.q.x = float2fixed(dev_rect.q.x);
    pxfmst->dev_print_rect.q.y = float2fixed(dev_rect.q.y);

    pcl_invert_mtx(&(pxfmst->lp2pg_mtx), &pg2lp);
    pcl_transform_rect(&print_rect, &(pxfmst->lp_print_rect), &pg2lp);

    /* restablish the current point and text region */
    restore_cap_and_margins(pcs, &cur_pt, &text_rect);

    /*
     * No need to worry about pat_orient or pat_ref_pt; these will always
     * be recalculated just prior to use.
     */
}


/* default margins, relative to the logical page boundaries */
#define DFLT_TOP_MARGIN     inch2coord(0.5)
#define DFLT_LEFT_MARGIN    inch2coord(0.0)
#define DFLT_RIGHT_MARGIN   inch2coord(0.0)
#define DFLT_BOTTOM_MARGIN  inch2coord(0.5)

#define TOP_MARGIN(hgt, tmarg)  ((hgt) > (tmarg) ? (tmarg) : 0)
#define PAGE_LENGTH(hgt, bmarg) ((hgt) > (bmarg) ? (hgt) - (bmarg) : (hgt))

/*
 * Reset the top margin an text length.
 *
 * Note that, even though changing the print direction merely relabels (but does
 * not relocate) the margins, the preint direction does change the location of
 * the default margins.
 */
  private void
reset_vertical_margins(
    pcl_state_t *   pcs
)
{
    pcl_margins_t * pmar = &(pcs->margins);
    coord           hgt = pcs->xfm_state.pd_size.y;

    pmar->top = TOP_MARGIN(hgt, DFLT_TOP_MARGIN);
    pmar->length = PAGE_LENGTH(hgt - pmar->top, DFLT_BOTTOM_MARGIN);
}

/*
 * Reset horizontal margins
 *
 * Note that, even though changing the print direction merely relabels (but does
 * not relocate) the margins, the preint direction does change the location of
 * the default margins.
 */
  private void
reset_horizontal_margins(
    pcl_state_t *   pcs
)
{
    pcl_margins_t * pmar = &(pcs->margins);

    pmar->left = DFLT_LEFT_MARGIN;
    pmar->right = pcs->xfm_state.pd_size.x - DFLT_RIGHT_MARGIN;
}

/*
 * Reset both the horizontal and vertical margins
 */
  private void
reset_margins(
    pcl_state_t *   pcs
)
{
    reset_horizontal_margins(pcs);
    reset_vertical_margins(pcs);
}

/*
 * Reset all parameters which must be reset whenever the page size changes.
 *
 * The third operand indicates if this routine is being called as part of
 * an initial reset. In that case, done't call HPGL's reset - the reset
 * will do that later.
 */
  void
new_page_size(
    pcl_state_t *               pcs,
    const pcl_paper_size_t *    psize,
    bool                        reset_initial
)
{
    floatp                      width_pts = psize->width * 0.01;
    floatp                      height_pts = psize->height * 0.01;
    float                       page_size[2];
    gs_state *                  pgs = pcs->pgs;
    gs_matrix                   mat;

    page_size[0] = width_pts;
    page_size[1] = height_pts;
    put_param1_float_array(pcs, "PageSize", page_size);

    /*
     * Reset the default transformation.
     *
     * The graphic library provides a coordinate system in points, with the
     * origin at the lower left corner of the page. The PCL code uses a
     * coordinate system in centi-points, with the origin at the upper left
     * corner of the page.
     */
    gs_setdefaultmatrix(pgs, NULL);
    gs_initmatrix(pgs);
    gs_currentmatrix(pgs, &mat);
    gs_matrix_translate(&mat, 0.0, height_pts, &mat);
    gs_matrix_scale(&mat, 0.01, -0.01, &mat);
    gs_setdefaultmatrix(pgs, &mat);

    pcs->xfm_state.paper_size = psize;
    pcs->overlay_enabled = false;
    update_xfm_state(pcs);
    reset_margins(pcs);

    /* 
     * If this is an initial reset, make sure underlining is disabled (homing
     * the cursor may cause an underline to be put out.
     */
    if (reset_initial)
        pcs->underline_enabled = false;
    pcl_home_cursor(pcs);

    pcl_xfm_reset_pcl_pat_ref_pt(pcs);

    /* the following sometimes erroneosuly sets have_page */
    if (!reset_initial)
        hpgl_do_reset(pcs, pcl_reset_page_params);
    gs_erasepage(pcs->pgs);
    pcs->have_page = false;
}

/*
 * Reset all parameters which must be reset whenever the logical page
 * orientation changes.
 *
 * The last operand indicates if this routine is being called as part of
 * an initial resete.
 */
  private void
new_logical_page(
    pcl_state_t *               pcs,
    int                         lp_orient,
    const pcl_paper_size_t *    psize,
    bool                        reset_initial
)
{
    pcl_xfm_state_t *           pxfmst = &(pcs->xfm_state);

    pcs->hmi_cp = HMI_DEFAULT;
    pcs->vmi_cp = VMI_DEFAULT;
    pxfmst->lp_orient = lp_orient;
    pxfmst->print_dir = 0;
    new_page_size(pcs, psize, reset_initial);
}


/* page end function to use */
private int     (*end_page)( pcl_state_t * pcs, int num_copies, int flush );

/*
 * Set the page output procedure.
 */
  void
pcl_set_end_page(
    int     (*procp)( pcl_state_t * pcs, int num_copies, int flush )
)
{
    end_page = procp;
}

/*
 * Stub output page procedure. This will normally be overridden in pcmain.c.
 */
  private int
default_end_page(
    pcl_state_t *   pcs,
    int             num_copies,
    int             flush
)
{
    return gs_output_page(pcs->pgs, num_copies, flush);
}

/*
 * End a page, either unconditionally or only if there are marks on it.
 * Return 1 if the page was actually printed and erased.
 */
  int
pcl_end_page(
    pcl_state_t *           pcs,
    pcl_print_condition_t   condition
)
{	
    int                     code = 0;

    pcl_break_underline(pcs);	/* (could mark page) */

    if (condition != pcl_print_always) {
	gx_device * dev = gs_currentdevice(pcs->pgs);

	/* Check whether there are any marks on the page. */
	if (!pcs->have_page)
	    return 0;	/* definitely no marks */

	/* Check whether we're working with a bbox device. */
	if (strcmp(gs_devicename(dev), "bbox") == 0) {
	    gs_rect bbox;

	    /*
             * A bbox device can give us a definitive answer;
	     * (otherwise we have to be conservative).
             */
	    gx_device_bbox_bbox((gx_device_bbox *)dev, &bbox);
	    if ((bbox.p.x >= bbox.q.x) || (bbox.p.y >= bbox.q.y))
		return 0;
	}
    }

    /* If there's an overlay macro, execute it now. */
    if (pcs->overlay_enabled) {
        void *  value;

        if ( pl_dict_find( &pcs->macros,
      		           id_key(pcs->overlay_macro_id),
                           2,
			   &value
                           ) ) {
	    pcs->overlay_enabled = false;   /**** IN reset_overlay ****/
	    code = pcl_execute_macro( (const pcl_macro_t *)value,
                                      pcs,
      				      pcl_copy_before_overlay,
				      pcl_reset_overlay,
				      pcl_copy_after_overlay
                                      );
	    pcs->overlay_enabled = true; /**** IN copy_after ****/
	}
    }

    /* output the page */
    (*end_page)(pcs, pcs->num_copies, true);
    pcl_set_drawing_color(pcs, pcl_pattern_solid_white, 0, false);
    code = gs_erasepage(pcs->pgs);
    pcs->have_page = false;

    /*
     * Advance of a page may move from a page front to a page back. This may
     * change the applicable transformations.
     */
    update_xfm_state(pcs);

    pcl_continue_underline(pcs);

    return (code < 0 ? code : 1);
}


/* Commands */

/*
 * Define the known paper sizes.
 *
 * The values are taken from the H-P manual and are in 1/300" units,
 * but the structure values are in centipoints (1/7200").
 */
#define p_size(t, n, w, h, offp, offl)                                  \
    { (t), (n), { (w) * 24L, (h) * 24L, (offp) * 24L, (offl) * 24L } }

private struct {
    uint                    tag;
    const char *            pname;
    pcl_paper_size_t        psize;
} paper_sizes[] = {
    p_size(  1, "executive", 2175, 3150, 75, 60),
    p_size(  2, "letter",    2550, 3300, 75, 60),
    p_size(  3, "legal",     2550, 4200, 75, 60),
    p_size(  6, "ledger",    3300, 5100, 75, 60),
    p_size( 26, "a4",        2480, 3507, 71, 59),
    p_size( 27, "a3",        3507, 4960, 71, 59),
    p_size( 80, "monarch",   1162, 2250, 75, 60),
    p_size( 81, "com_10",    1237, 2850, 75, 60),
    p_size( 90, "dl",        1299, 2598, 71, 59),
    p_size( 91, "c5",        1913, 2704, 71, 59),
    p_size(100, "b5",        2078, 2952, 71, 59)
};

/*
 * ESC & l <psize_enum> A
 *
 * Select paper size
 */
  private int
set_page_size(
    pcl_args_t *                pargs,
    pcl_state_t *               pcs
)
{
    /* Note: not all values are implemented on all printers. */
    uint                        tag = uint_arg(pargs);
    int                         i;
    int                         code = 0;
    const pcl_paper_size_t *    psize = 0;

    for (i = 0; i < countof(paper_sizes); i++) {
        if (tag == paper_sizes[i].tag) {
            psize = &(paper_sizes[i].psize);
            break;
        }
    }
    if ((psize != 0) && ((code = pcl_end_page_if_marked(pcs)) >= 0))
        new_page_size(pcs, psize, false);
    return code;
}

/*
 * ESC & l <feed_enum> H
 *
 * Set paper source
 */
  private int
set_paper_source(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    /* Note: not all printers support all possible values. */
    if (i <= 6) {
        int     code = pcl_end_page_if_marked(pcs);

        if ((i > 0) && (code >= 0))
            code = put_param1_int(pcs, "%MediaSource", i);
        pcl_home_cursor(pcs);
        return (code < 0 ? code : 0);
    } else
        return e_Range;

}

/*
 * ESC & l <xoff_dp> U
 *
 * Note that this shifts the logical page, but does NOT change its size nor
 * does it reset any logical-page related parameters.
 */
  private int
set_left_offset_registration(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->xfm_state.left_offset_cp = float_arg(pargs) * 10.0;
    update_xfm_state(pcs);
    return 0;
}

/*
 * ESC & l <yoff_dp> Z
 *
 * Note that this shifts the logical page, but does NOT change its size nor
 * does it reset any logical-page related parameters.
 */
  private int
set_top_offset_registration(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->xfm_state.top_offset_cp = float_arg(pargs) * 10;
    update_xfm_state(pcs);
    return 0;
}

/*
 * ESC & l <orient> O
 *
 * Set logical page orientation. In apparent contradiction to TRM 5-6,
 * setting the orientation to the same value is a no-op.
 */
  private int
set_logical_page_orientation(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);
    int             code = 0;

    if ( (i <= 3)                                   &&
         (i != pcs->xfm_state.lp_orient)            &&
         ((code = pcl_end_page_if_marked(pcs)) >= 0)  )
        new_logical_page(pcs, i, pcs->xfm_state.paper_size, false);
    return code;
}

/*
 * ESC & a <angle> P
 */
  private int
set_print_direction(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if ((i <= 270) && (i % 90 == 0)) {
        pcs->xfm_state.print_dir = i / 90;
        update_xfm_state(pcs);
    }
    return 0;
}

/*
 * ESC & a <col> L
 *
 * Set left margin.
 */
  private int
set_left_margin(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           lmarg = uint_arg(pargs) * pcl_hmi(pcs);

    if (lmarg < pcs->margins.right) {
        pcs->margins.left = lmarg;
        if (pcl_cap.x < lmarg)
            pcl_set_cap_x(pcs, lmarg, false, false);
    }
    return 0;
}

/*
 * ESC & a <col> M
 *
 * Set right margin. The right margin is set to the *right* edge of the
 * specified column, so we need to add 1 to the column number.
 */
  private int
set_right_margin(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           rmarg = (uint_arg(pargs) + 1) * pcl_hmi(pcs);

    if (rmarg > pcs->xfm_state.pd_size.x)
	rmarg = pcs->xfm_state.pd_size.x;
    if (rmarg > pcs->margins.left) {
        pcs->margins.right = rmarg;
        if (pcl_cap.x > rmarg)
            pcl_set_cap_x(pcs, rmarg, false, false);
    }

    return 0;
}

/*
 * ESC 9
 *
 * Clear horizontal margins.
 */
  private int
clear_horizontal_margins(
    pcl_args_t *    pargs,  /* ignored */
    pcl_state_t *   pcs
)
{
    reset_horizontal_margins(pcs);
    return 0;
}

/*
 * ESC & l <line> E
 *
 * Set top margin. This will also reset the page length.
 */
  private int
set_top_margin(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           hgt = pcs->xfm_state.pd_size.y;
    coord           tmarg = uint_arg(pargs) * pcs->vmi_cp;

    if ((pcs->vmi_cp != 0) && (tmarg <= hgt)) {
        pcs->margins.top = tmarg;
        pcs->margins.length = PAGE_LENGTH(hgt - tmarg, DFLT_BOTTOM_MARGIN);
    }
    return 0;
}

/*
 * ESC & l <lines> F
 *
 * Set text length (which indirectly sets the bottom margin).
 */
  private int
set_text_length(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           len = uint_arg(pargs) * pcs->vmi_cp;

    if ((len != 0) && (pcs->margins.top + len <= pcs->xfm_state.pd_size.y) )
	pcs->margins.length = len;
    return 0;
}

/*
 * ESC & l <enable> L
 *
 * Set perferation skip mode. Though performation skip is more closely related
 * to vertical motion than to margins, the command is included here because it
 * resets the vertical margins (top margin and text length) to their defaults.
 */
  private int
set_perforation_skip(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    bool            new_skip = uint_arg(pargs);

    if ((new_skip != pcs->perforation_skip) && (new_skip <= 1)) {
        reset_vertical_margins(pcs);
	pcs->perforation_skip = new_skip;
    }
    return 0;
}

/* 
 * (From PCL5 Comparison Guide, p. 1-98)
 *
 * ESC & l <type> M
 *
 * Set media type.
 */
  private int
pcl_media_type(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             type = uint_arg(pargs);

    if (type <= 4) {
        int     code = pcl_end_page_if_marked(pcs);

        if (code >= 0)
            pcl_home_cursor(pcs);
        return (code < 0 ? code : e_Unimplemented);
    } else
        return e_Range;
}

/*
 * (From PCL5 Comparison Guide, p. 1-99)
 *
 * ESC * o <quality> Q 
 *
 * Set print quality.
 */
  private int
pcl_print_quality(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    int             quality = int_arg(pargs);

    if ((quality >= -1) && (quality <= 1)) {
        int     code = pcl_end_page_if_marked(pcs);

        if (code >= 0)
            pcl_home_cursor(pcs);
        return (code < 0 ? code : 0);
    } else
	return e_Range;
}

/*
 * Initialization
 */
  private int
pcpage_do_init(
    gs_memory_t *   pmem    /* ignored */
)
{
    /* Register commands */
    DEFINE_CLASS('&')
    {
        'l', 'A',
	PCL_COMMAND( "Page Size",
                     set_page_size,
                     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'H',
	PCL_COMMAND( "Paper Source",
                     set_paper_source,
                     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'U',
        PCL_COMMAND( "Left Offset Registration",
		     set_left_offset_registration,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'Z',
        PCL_COMMAND( "Top Offset Registration",
		     set_top_offset_registration,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'O',
        PCL_COMMAND( "Page Orientation",
                     set_logical_page_orientation,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'a', 'P',
        PCL_COMMAND( "Print Direction",
                     set_print_direction,
                     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'a', 'L',
	PCL_COMMAND( "Left Margin",
                     set_left_margin,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'a', 'M',
        PCL_COMMAND( "Right Margin",
                     set_right_margin,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'E',
	PCL_COMMAND( "Top Margin",
                     set_top_margin,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'F',
	PCL_COMMAND( "Text Length",
                     set_text_length,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'L',
	PCL_COMMAND( "Perforation Skip",
                     set_perforation_skip,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    {
        'l', 'M',
	PCL_COMMAND( "Media Type",
                     pcl_media_type,
		     pca_neg_ok | pca_big_ignore
                     )
    },
    END_CLASS

    DEFINE_ESCAPE( '9',
                   "Clear Horizontal Margins",
                   clear_horizontal_margins
                   )

    DEFINE_CLASS_COMMAND_ARGS( '*',
                               'o',
                               'Q',
                               "Print Quality",
			       pcl_print_quality,
                               pca_neg_ok | pca_big_ignore
                               )
    return 0;
}

 private pcl_paper_size_t *
get_default_paper(
    pcl_state_t *      pcs
)
{
    int i;
    char *psize = pjl_get_envvar(pcs->pjls, "paper");
    pcs->wide_a4 = false;
    for (i = 0; i < countof(paper_sizes); i++)
        if (!pjl_compare(psize, paper_sizes[i].pname)) {
	    /* we are not sure if widea4 applies to all paper sizes */
	    if (!pjl_compare(pjl_get_envvar(pcs->pjls, "widea4"), "YES"))
		pcs->wide_a4 = true;
	    return &(paper_sizes[i].psize);
	}
    dprintf2("pcl does not support system requested %s paper setting %s\n",
	     psize, paper_sizes[1].pname);
    return &(paper_sizes[1].psize);
}
    
  private void
pcpage_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{
    if ((type & (pcl_reset_initial | pcl_reset_printer)) != 0) {
        if ((type & pcl_reset_initial) != 0)
	    end_page = default_end_page;
	pcs->paper_source = 0;		/* ??? */
        pcs->xfm_state.left_offset_cp = 0.0;
        pcs->xfm_state.top_offset_cp = 0.0;
	pcs->perforation_skip = 1;
        new_logical_page( pcs,
			  !pjl_compare(pjl_get_envvar(pcs->pjls,
						 "orientation"),
				       "portrait") ? 0 : 1,
                          get_default_paper(pcs),
                          (type & pcl_reset_initial) != 0
                          );
	pcs->have_page = false;

    } else if ((type & pcl_reset_overlay) != 0) {
	pcs->perforation_skip = 1;
        pcs->xfm_state.print_dir = 0;
        update_xfm_state(pcs);
        reset_margins(pcs);
        pcl_xfm_reset_pcl_pat_ref_pt(pcs);
    }
}

const pcl_init_t    pcpage_init = { pcpage_do_init, pcpage_do_reset, 0 };
