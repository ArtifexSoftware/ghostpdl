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

/* pcpage.c - PCL5 page and transformation control commands */

#include "math_.h"
#include "std.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcdraw.h"
#include "pcparam.h"
#include "pcparse.h"            /* for pcl_execute_macro */
#include "pcfont.h"             /* for underline interface */
#include "pcpatxfm.h"
#include "pcursor.h"
#include "pcpage.h"
#include "pgmand.h"
#include "pginit.h"
#include "plmain.h"             /* for finish page */
#include "gsmatrix.h"           /* for gsdevice.h */
#include "gsnogc.h"
#include "gscoord.h"
#include "gsdevice.h"
#include "gspaint.h"
#include "gspath.h"
#include "gxdevice.h"
#include "pjtop.h"

/*
 * The PCL printable region. HP always sets the boundary of this region to be
 * 1/6" in from the edge of the paper, irrespecitve of the paper size or the
 * device. Since this dimension affects some default values related to rasters,
 * it is important that it be assigned this value.
 */
#define PRINTABLE_MARGIN_CP inch2coord(1.0 / 6.0)

#define round(x)    (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))


/* Procedures */

/*
 * Preserve the current point and text margin set by transfroming them into
 * logical page space.
 */
  static void
preserve_cap_and_margins(
    const pcl_state_t *  pcs,
    gs_point *           pcur_pt,
    gs_rect *            ptext_rect
)
{
    pcur_pt->x = (double)pcs->cap.x;
    pcur_pt->y = (double)pcs->cap.y;
    gs_point_transform( pcur_pt->x,
                        pcur_pt->y,
                        &(pcs->xfm_state.pd2lp_mtx),
                        pcur_pt
                        );
    ptext_rect->p.x = (double)pcs->margins.left;
    ptext_rect->p.y = (double)pcs->margins.top;
    ptext_rect->q.x = (double)pcs->margins.right;
    ptext_rect->q.y = (double)(pcs->margins.top + pcs->margins.length);
    pcl_transform_rect(pcs->memory, ptext_rect, ptext_rect, &(pcs->xfm_state.pd2lp_mtx));
}

/*
 * Convert the current point and text margin set back into "pseudo print
 * direction" space.
 */
  static void
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
    pcs->cap.x = (coord)tmp_pt.x;
    pcs->cap.y = (coord)tmp_pt.y;
    pcl_transform_rect(pcs->memory, ptext_rect, &tmp_rect, &lp2pd);
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
  static void
update_xfm_state(
    pcl_state_t *               pcs,
    bool                        reset_initial
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
    if ( !reset_initial )
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
    if ( pcs->personality == rtl )
        offset = 0;
    else
        offset = ( (pxfmst->lp_orient & 0x1) != 0 ? psize->offset_landscape
                   : psize->offset_portrait );

    /* we need an extra 1/10 inch on each side to support 80
       characters vs. 78 at 10 cpi.  Only apply to A4. */
    if ( ( pcs->wide_a4 ) &&
         (psize->width == 59520) &&
         (psize->height == 84168) )
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
    pg2dev.ty = round(pg2dev.ty); pg2dev.tx = round(pg2dev.tx);
    pxfmst->lp2dev_mtx.tx = round(pxfmst->lp2dev_mtx.tx);
    pxfmst->lp2dev_mtx.ty = round(pxfmst->lp2dev_mtx.ty);
    /* the "pseudo page direction to logical page/device transformations */
    pcl_make_rotation( pxfmst->print_dir,
                       (floatp)pxfmst->lp_size.x,
                       (floatp)pxfmst->lp_size.y,
                       &(pxfmst->pd2lp_mtx)
                       );
    gs_matrix_multiply( &(pxfmst->pd2lp_mtx),
                        &(pxfmst->lp2dev_mtx),
                        &(pxfmst->pd2dev_mtx)
                        );

    /* calculate the print direction page size */
    if ((pxfmst->print_dir) & 0x1) {
        pxfmst->pd_size.x = pxfmst->lp_size.y;
        pxfmst->pd_size.y = pxfmst->lp_size.x;
    } else
        pxfmst->pd_size = pxfmst->lp_size;

    {
        gx_device *pdev = gs_currentdevice(pcs->pgs);
        /* We must not set up a clipping region beyond the hardware margins of
           the device, but the pcl language definition requires hardware
           margins to be 1/6".  We set all margins to the the maximum of the
           PCL language defined 1/6" and the actual hardware margin.  If 1/6"
           is not available pcl will not work correctly all of the time. */
        if ( pcs->personality == rtl ) {
            print_rect.p.x = inch2coord(pdev->HWMargins[0] / 72.0);
            print_rect.p.y = inch2coord(pdev->HWMargins[1]) / 72.0;
            print_rect.q.x = psize->width - inch2coord(pdev->HWMargins[2] / 72.0);
            print_rect.q.y = psize->height - inch2coord(pdev->HWMargins[3] / 72.0);
        } else {
            print_rect.p.x = max(PRINTABLE_MARGIN_CP, inch2coord(pdev->HWMargins[0] / 72.0));
            print_rect.p.y = max(PRINTABLE_MARGIN_CP, inch2coord(pdev->HWMargins[1]) / 72.0);
            print_rect.q.x = psize->width - max(PRINTABLE_MARGIN_CP, inch2coord(pdev->HWMargins[2] / 72.0));
            print_rect.q.y = psize->height - max(PRINTABLE_MARGIN_CP, inch2coord(pdev->HWMargins[3] / 72.0));
        }
        pcl_transform_rect(pcs->memory, &print_rect, &dev_rect, &pg2dev);
        pxfmst->dev_print_rect.p.x = float2fixed(round(dev_rect.p.x));
        pxfmst->dev_print_rect.p.y = float2fixed(round(dev_rect.p.y));
        pxfmst->dev_print_rect.q.x = float2fixed(round(dev_rect.q.x));
        pxfmst->dev_print_rect.q.y = float2fixed(round(dev_rect.q.y));
    }
    pcl_invert_mtx(&(pxfmst->lp2pg_mtx), &pg2lp);
    pcl_transform_rect(pcs->memory, &print_rect, &(pxfmst->lp_print_rect), &pg2lp);

    /* restablish the current point and text region */
    if ( !reset_initial )
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

#define DFLT_TOP_MARGIN_PASSTHROUGH inch2coord(1.0/6.0)
#define DFLT_BOTOM_MARGIN_PASSTHROUGH inch2coord(1.0/6.0)

#define TOP_MARGIN(hgt, tmarg)  ((hgt) > (tmarg) ? (tmarg) : 0)
#define PAGE_LENGTH(hgt, bmarg) ((hgt) > (bmarg) ? (hgt) - (bmarg) : (hgt))

/*
 * Reset the top margin an text length.
 *
 * Note that, even though changing the print direction merely relabels (but does
 * not relocate) the margins, the preint direction does change the location of
 * the default margins.
 */
  static void
reset_vertical_margins(
      pcl_state_t *   pcs,
      bool            for_passthrough
)
{
    pcl_margins_t * pmar = &(pcs->margins);
    coord           hgt = pcs->xfm_state.pd_size.y;
    coord           tm = (for_passthrough ?
                          DFLT_TOP_MARGIN_PASSTHROUGH : DFLT_TOP_MARGIN);
    coord           bm = (for_passthrough ?
                          DFLT_BOTOM_MARGIN_PASSTHROUGH : DFLT_BOTTOM_MARGIN);

    pmar->top = TOP_MARGIN(hgt, tm);
    pmar->length = PAGE_LENGTH(hgt - pmar->top, bm);
}

/*
 * Reset horizontal margins
 *
 * Note that, even though changing the print direction merely relabels (but does
 * not relocate) the margins, the preint direction does change the location of
 * the default margins.
 */
  static void
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
static void
reset_margins(
    pcl_state_t *   pcs,
    bool            for_passthrough
)
{
    reset_horizontal_margins(pcs);
    reset_vertical_margins(pcs, for_passthrough);
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
    bool                        reset_initial,
    bool                        for_passthrough
)
{
    floatp                      width_pts = psize->width * 0.01;
    floatp                      height_pts = psize->height * 0.01;
    float                       page_size[2];
    static float                old_page_size[2] = { 0, 0 };
    gs_state *                  pgs = pcs->pgs;
    gs_matrix                   mat;

    page_size[0] = width_pts;
    page_size[1] = height_pts;

    /* NB: save off the page size for the case where set device page size
     * doesn't match the PJL page size and no escE reset occurs, and we are not coming from PXL.
     * A direct query, reset initial erasepage or a erasepage with notmarked don't do it
     * would all be preferable...
     */
    old_page_size[0] = pcs->xfm_state.paper_size ? pcs->xfm_state.paper_size->width : 0;
    old_page_size[1] = pcs->xfm_state.paper_size ? pcs->xfm_state.paper_size->height : 0;

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
    update_xfm_state(pcs, reset_initial);
    reset_margins(pcs, for_passthrough);

    /*
     * If this is an initial reset, make sure underlining is disabled (homing
     * the cursor may cause an underline to be put out.
     */
    if (reset_initial)
        pcs->underline_enabled = false;
    else
        pcl_home_cursor(pcs);

    pcl_xfm_reset_pcl_pat_ref_pt(pcs);

    if (!reset_initial) {
        hpgl_do_reset(pcs, pcl_reset_page_params);
        if ( pcs->end_page == pcl_end_page_top )   /* don't erase in snippet mode */
            gs_erasepage(pcs->pgs);
        pcs->page_marked = false;
    }
    else if ( pcs->end_page == pcl_end_page_top &&
              !(old_page_size[0] == pcs->xfm_state.paper_size->width &&
                old_page_size[1] == pcs->xfm_state.paper_size->height)) {
            /* PJL paper size change without escE case. */
        gs_erasepage(pcs->pgs);
        pcs->page_marked = false;
    }
}

/*
 * Define the known paper sizes.
 *
 * The values are taken from the H-P manual and are in 1/300" units,
 * but the structure values are in centipoints (1/7200").
 */
#define p_size(t, n, w, h, offp, offl)                                  \
    { (t), (n), { (w) * 24L, (h) * 24L, (offp) * 24L, (offl) * 24L } }

static struct {
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
 * Reset all parameters which must be reset whenever the logical page
 * orientation changes.
 *
 * The last operand indicates if this routine is being called as part of
 * an initial resete.
 */
  void
new_logical_page(
    pcl_state_t *               pcs,
    int                         lp_orient,
    const pcl_paper_size_t *    psize,
    bool                        reset_initial,
    bool                        for_passthrough
)
{
    pcl_xfm_state_t *           pxfmst = &(pcs->xfm_state);

    pxfmst->lp_orient = lp_orient;
    pxfmst->print_dir = 0;
    new_page_size(pcs, psize, reset_initial, for_passthrough);
}

int
pcl_new_logical_page_for_passthrough(pcl_state_t *pcs, int orient, int tag)
{
    int i;
     pcl_paper_size_t *psize;

    for (i = 0; i < countof(paper_sizes); i++) {
        if (tag == paper_sizes[i].tag) {
            psize = &(paper_sizes[i].psize);
            break;
        }
    }
    if (psize == 0)
        return -1;
    new_logical_page(pcs, orient, psize, false, true);
    return 0;

}



/* page marking routines */

/* set page marked for path drawing commands.  NB doesn't handle 0 width - lenghth */
void
pcl_mark_page_for_path(pcl_state_t *pcs)
{
    if ( pcs->page_marked )
        return;
    {
        gs_rect bbox;
        bbox.p.x = bbox.p.y = bbox.q.x = bbox.q.y = 0;
        gs_pathbbox(pcs->pgs, &bbox);
        if ((bbox.p.x < bbox.q.x) && (bbox.p.y < bbox.q.y))
            pcs->page_marked = true;
        return;
    }
}

void
pcl_mark_page_for_current_pos(pcl_state_t *pcs)
{
    /* nothing to do */
    if ( pcs->page_marked )
        return;

    /* convert current point to device space and check if it is inside
       device rectangle for the page */
    {
        gs_fixed_rect page_bbox_fixed = pcs->xfm_state.dev_print_rect;
        gs_rect page_bbox_float;
        gs_point current_pt, dev_pt;

        page_bbox_float.p.x = fixed2float(page_bbox_fixed.p.x);
        page_bbox_float.p.y = fixed2float(page_bbox_fixed.p.y);
        page_bbox_float.q.x = fixed2float(page_bbox_fixed.q.x);
        page_bbox_float.q.y = fixed2float(page_bbox_fixed.q.y);

        if ( gs_currentpoint(pcs->pgs, &current_pt) < 0 ) {
             dprintf("Not expected to fail\n" );
             return;
        }

        if ( gs_transform(pcs->pgs, current_pt.x, current_pt.y, &dev_pt) ) {
             dprintf("Not expected to fail\n" );
             return;
        }

        /* half-open lower - not sure this is correct */
        if ( dev_pt.x >= page_bbox_float.p.x &&
             dev_pt.y >= page_bbox_float.p.y &&
             dev_pt.x < page_bbox_float.q.x &&
             dev_pt.y < page_bbox_float.q.y )
            pcs->page_marked = true;

    }
}

/* returns the bounding box coordinates for the current device and a
   boolean to indicate marked status. 0 - unmarked 1 - marked -1 error */
 int
pcl_page_marked(
    pcl_state_t *           pcs
)
{
    return pcs->page_marked;
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

    pcl_break_underline(pcs);   /* (could mark page) */

    /* If we are conditionally printing (normal case) check if the
       page is marked */
    if (condition != pcl_print_always) {
        if ( !pcl_page_marked(pcs) )
            return 0;
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
    code = (*pcs->end_page)(pcs, pcs->num_copies, true);
    if ( code < 0 )
        return code;
    /* allow the logical orientation command to be used again */
    pcs->orientation_set = false;

    if ( pcs->end_page == pcl_end_page_top )
        code = gs_erasepage(pcs->pgs);
    pcs->page_marked = false;
    /* force new logical page, allows external resolution changes.
     * see -dFirstPage -dLastPage
     * NB would be faster if we didn't do this every page.
     *
     * NB setting a new logical page defaults settings
     * that should carry over from the previous page
     * this error occurs only on documents that don't do any initilizations per page
     * hence only the viewer applications will see the speedup and the error
     */
    if (!pjl_proc_compare(pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "viewer"), "on")) {
        new_logical_page(pcs, pcs->xfm_state.lp_orient,
                         pcs->xfm_state.paper_size, false, false);
    }

    /*
     * Advance of a page may move from a page front to a page back. This may
     * change the applicable transformations.
     */
    update_xfm_state(pcs, 0);

    pcl_continue_underline(pcs);
    return (code < 0 ? code : 1);
}


/* Commands */

/*
 * ESC & l <psize_enum> A
 *
 * Select paper size
 */
  static int
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

    /* oddly the command goes to the next page irrespective of
       arguments */
    code = pcl_end_page_if_marked(pcs);
    if ( code < 0 )
        return code;
    pcl_home_cursor(pcs);

    for (i = 0; i < countof(paper_sizes); i++) {
        if (tag == paper_sizes[i].tag) {
            psize = &(paper_sizes[i].psize);
            break;
        }
    }
    if ((psize != 0) && ((code = pcl_end_page_if_marked(pcs)) >= 0)) {
        /* if the orientation flag is not set for this page we select
           a portrait page using the set paper size.  Otherwise select
           the paper using the current orientation. */
        if ( pcs->orientation_set == false )
            new_logical_page(pcs, 0, psize, false, false);
        else
            new_page_size(pcs, psize, false, false);
    }
    return code;
}

/*
 * ESC & l <feed_enum> H
 *
 * Set paper source
 */
  static int
set_paper_source(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    /* oddly the command goes to the next page irrespective of
       arguments */
    int code = pcl_end_page_if_marked(pcs);
    if ( code < 0 )
        return code;
    pcl_home_cursor(pcs);

    /* Note: not all printers support all possible values. */
    if (i <= 6) {
        code = 0;
        if (i > 0)
            code = put_param1_int(pcs, "%MediaSource", i);
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
  static int
set_left_offset_registration(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->xfm_state.left_offset_cp = float_arg(pargs) * 10.0;
    update_xfm_state(pcs, 0);
    return 0;
}

/*
 * ESC & l <yoff_dp> Z
 *
 * Note that this shifts the logical page, but does NOT change its size nor
 * does it reset any logical-page related parameters.
 */
  static int
set_top_offset_registration(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    pcs->xfm_state.top_offset_cp = float_arg(pargs) * 10;
    update_xfm_state(pcs, 0);
    return 0;
}

/*
 * ESC & l <orient> O
 *
 * Set logical page orientation.
 */
  static int
set_logical_page_orientation(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);
    int             code;

    /* the command is ignored if it is value is out of range */
    if ( i > 3 )
        return 0;

    /* this command is ignored in pcl xl snippet mode.  NB we need a
       better flag for snippet mode. */
    if (pcs->end_page != pcl_end_page_top) {
        return 0;
    }

    /* If orientation is same as before ignore the command */
    if ( i == pcs->xfm_state.lp_orient ) {
        pcs->orientation_set = true;
        return 0;
    }

    /* ok to execute - clear the page, set up the transformations and
       set the flag disabling the orientation command for this page. */
    code = pcl_end_page_if_marked(pcs);
    if ( code >= 0 ) {
        pcs->hmi_cp = HMI_DEFAULT;
        pcs->vmi_cp = VMI_DEFAULT;
        new_logical_page(pcs, i, pcs->xfm_state.paper_size, false, false);
        pcs->orientation_set = true;
    }
    return code;
}

/*
 * ESC & a <angle> P
 */
  static int
set_print_direction(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint            i = uint_arg(pargs);

    if ((i <= 270) && (i % 90 == 0)) {
        i /= 90;
        if (i !=  pcs->xfm_state.print_dir) {
            pcl_break_underline(pcs);
            pcs->xfm_state.print_dir = i;
            update_xfm_state(pcs, 0);
            pcl_continue_underline(pcs);
        }
        else {
            pcs->xfm_state.print_dir = i;
            update_xfm_state(pcs, 0);
        }
    }
    return 0;
}

/*
 * ESC & a <col> L
 *
 * Set left margin.
 */
  static int
set_left_margin(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           lmarg = uint_arg(pargs) * pcl_hmi(pcs);

    /* adjust underlining if the left margin passes to the right of
       the underline start position */
    if ((pcs->underline_enabled) && (lmarg > pcs->underline_start.x))
        pcs->underline_start.x = lmarg;

    if (lmarg < pcs->margins.right) {
        pcs->margins.left = lmarg;
        if (pcs->cap.x < lmarg)
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
  static int
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
        if (pcs->cap.x > rmarg)
            pcl_set_cap_x(pcs, rmarg, false, false);
    }

    return 0;
}

/*
 * ESC 9
 *
 * Clear horizontal margins.
 */
  static int
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
  static int
set_top_margin(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           hgt = pcs->xfm_state.pd_size.y;
    coord           tmarg = uint_arg(pargs) * pcs->vmi_cp;

    if ((pcs->vmi_cp != 0) && (tmarg <= hgt)) {
        bool was_default = (pcs->margins.top == TOP_MARGIN(hgt, DFLT_TOP_MARGIN) &&
                            pcs->margins.left == DFLT_LEFT_MARGIN);

        pcs->margins.top = tmarg;
        pcs->margins.length = PAGE_LENGTH(hgt - tmarg, DFLT_BOTTOM_MARGIN);

        /* If the cursor has been moved then we have a fixed cap and the
           top margin only affects the next page. If the cap is floating,
           unmarked and unmoved, then the cap moves to the first line of text.
           More restrictive than implementor's guide:
           iff a default margin is changed on an unmarked unmoved page to the new margin.
           margin_set(A) ^L margin_set(B)  --> use (A)
           escE margin_set(B) --> use (B)
         */
        if (was_default && pcl_page_marked(pcs) == 0 )
            return pcl_set_cap_y(pcs, 0L, false, false, true, false);
    }
    return 0;
}

/*
 * ESC & l <lines> F
 *
 * Set text length (which indirectly sets the bottom margin).
 */
  static int
set_text_length(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    coord           len = uint_arg(pargs) * pcs->vmi_cp;

    if (len == 0) {
        len = pcs->xfm_state.pd_size.y - pcs->margins.top - inch2coord(1.0/2.0);
        if (len < 0) {
            len += inch2coord(1.0/2.0);
        }
    }
    if ((len >= 0) && (pcs->margins.top + len <= pcs->xfm_state.pd_size.y) )
        pcs->margins.length = len;
    return 0;
}

/*
 * ESC & l <enable> L
 *
 * Set perforation skip mode. Though performation skip is more closely related
 * to vertical motion than to margins, the command is included here because it
 * resets the vertical margins (top margin and text length) to their defaults.
 */
  static int
set_perforation_skip(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    bool            new_skip = uint_arg(pargs);

    if ((new_skip != pcs->perforation_skip) && (new_skip <= 1))
        pcs->perforation_skip = new_skip;
    return 0;
}

/*
 * (From PCL5 Comparison Guide, p. 1-98)
 *
 * ESC & l <type> M
 *
 * Set media type.
 */
  static int
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
 * Logical Page Setup (From HP D640 Technical Manual)
 */

typedef struct pcl_logical_page_s {
    byte LeftOffset[2];
    byte TopOffset[2];
    byte Orientation;
    byte Reserved;
    byte Width[2];
    byte Height[2];
} pcl_logical_page_t;

 static int
set_logical_page(
    pcl_args_t *    pargs,
    pcl_state_t *   pcs
)
{
    uint count = uint_arg(pargs);
    const pcl_logical_page_t *plogpage =
        (pcl_logical_page_t *)arg_data(pargs);

    if (count != 10)
        return e_Range;
    return 0;
}

/*
 * (From PCL5 Comparison Guide, p. 1-99)
 *
 * ESC * o <quality> Q
 *
 * Set print quality.
 */
  static int
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
  static int
pcpage_do_registration(
    pcl_parser_state_t *pcl_parser_state,
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
    {
        'a', 'W',
        PCL_COMMAND( "Set Logical Page",
                     set_logical_page,
                     pca_bytes
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

pcl_paper_size_t *
pcl_get_default_paper(
    pcl_state_t *      pcs
)
{
    int i;
    pjl_envvar_t *psize = pjl_proc_get_envvar(pcs->pjls, "paper");
    pcs->wide_a4 = false;
    for (i = 0; i < countof(paper_sizes); i++)
        if (!pjl_proc_compare(pcs->pjls, psize, paper_sizes[i].pname)) {
            /* set wide a4, only used if the paper is a4 */
            if (!pjl_proc_compare(pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "widea4"), "YES"))
                pcs->wide_a4 = true;
            return &(paper_sizes[i].psize);
        }
    dprintf("system does not support requested paper setting\n");
    return &(paper_sizes[1].psize);
}

  static void
pcpage_do_reset(
    pcl_state_t *       pcs,
    pcl_reset_type_t    type
)
{

    /* NB hack for snippet mode */
    if (pcs->end_page != pcl_end_page_top) {
        pcs->xfm_state.print_dir = 0;
        pcs->xfm_state.left_offset_cp = 0.0;
        pcs->xfm_state.top_offset_cp = 0.0;
        update_xfm_state(pcs, 0);

        /* if we are in snippet mode, ESC E (reset) will set the
           default margins otherwise we should use the unusual
           passthrough margins (1/6") see above */
        if (type & pcl_reset_printer)
            reset_margins(pcs, false);
        else
            reset_margins(pcs, true);
        return;
    }

    if ((type & (pcl_reset_initial | pcl_reset_printer)) != 0) {
        pcs->orientation_set = false;
        pcs->paper_source = 0;          /* ??? */
        pcs->xfm_state.left_offset_cp = 0.0;
        pcs->xfm_state.top_offset_cp = 0.0;
        pcs->perforation_skip = 1;
        new_logical_page( pcs,
                          !pjl_proc_compare(pcs->pjls, pjl_proc_get_envvar(pcs->pjls,
                                                 "orientation"),
                                       "portrait") ? 0 : 1,
                          pcl_get_default_paper(pcs),
                          (type & pcl_reset_initial) != 0,
                          false
                          );
    } else if ((type & pcl_reset_overlay) != 0) {
        pcs->perforation_skip = 1;
        update_xfm_state(pcs, 0);
        reset_margins(pcs, false);
        pcl_xfm_reset_pcl_pat_ref_pt(pcs);
    }
}

const pcl_init_t    pcpage_init = { pcpage_do_registration, pcpage_do_reset, 0 };
