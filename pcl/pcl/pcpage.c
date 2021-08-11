/* Copyright (C) 2001-2021 Artifex Software, Inc.
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
#include "plvalue.h"
#include "gsmatrix.h"           /* for gsdevice.h */
#include "gscoord.h"
#include "gsdevice.h"
#include "gspaint.h"
#include "gspath.h"
#include "gxdevice.h"
#include "pjtop.h"
#include "rtgmode.h"
#include <stdlib.h>             /* for atol() */

/*
 * The PCL printable region. In older HP printers this region was set to
 * 1/6" from the edge of the paper, irrespecitve of the paper size or the
 * device. However, recent HP printers allow the margin to be configurable
 * and even set to 0 for full bleed.  If a non-zero margin is needed it can be
 * set on the command line via -H (e.g. -H12x12x12x12 restores the previous
 * 1/6" margin).  See doc/ghostpdl.pdf for details.
 */
#define PRINTABLE_MARGIN_CP inch2coord(0.0)

#define round(x)    (((x) < 0.0) ? (ceil ((x) - 0.5)) : (floor ((x) + 0.5)))

/* Procedures */

/*
 * Preserve the current point and text margin set by transfroming them into
 * logical page space.
 */
static void
preserve_cap_and_margins(const pcl_state_t * pcs,
                         gs_point * pcur_pt, gs_rect * ptext_rect)
{
    pcur_pt->x = (double)pcs->cap.x;
    pcur_pt->y = (double)pcs->cap.y;
    gs_point_transform(pcur_pt->x,
                       pcur_pt->y, &(pcs->xfm_state.pd2lp_mtx), pcur_pt);
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
static void
restore_cap_and_margins(pcl_state_t * pcs,
                        const gs_point * pcur_pt, const gs_rect * ptext_rect)
{
    gs_matrix lp2pd;
    gs_point tmp_pt;
    gs_rect tmp_rect;

    pcl_invert_mtx(&(pcs->xfm_state.pd2lp_mtx), &lp2pd);
    gs_point_transform(pcur_pt->x, pcur_pt->y, &lp2pd, &tmp_pt);
    pcs->cap.x = (coord) tmp_pt.x;
    pcs->cap.y = (coord) tmp_pt.y;
    pcl_transform_rect(ptext_rect, &tmp_rect, &lp2pd);
    pcs->margins.left = (coord) tmp_rect.p.x;
    pcs->margins.top = (coord) tmp_rect.p.y;
    pcs->margins.right = (coord) tmp_rect.q.x;
    pcs->margins.length = (coord) tmp_rect.q.y - pcs->margins.top;
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
update_xfm_state(pcl_state_t * pcs, bool reset_initial)
{
    pcl_xfm_state_t *pxfmst = &(pcs->xfm_state);
    const pcl_paper_size_t *psize = pxfmst->paper_size;
    coord offset;
    gs_matrix pg2dev, pg2lp;
    gs_rect print_rect, dev_rect, text_rect;
    gs_point cur_pt;
    double loff = pxfmst->left_offset_cp;
    double toff = pxfmst->top_offset_cp;

    /* preserve the current point and text rectangle in logical page space */
    if (!reset_initial)
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
    {
        gx_device *pdev = gs_currentdevice(pcs->pgs);

        /* get the LeadingEdge from the device
         * This has probably no influence on the resulting calculations,
         * as the offsetregistration is only influenced by pageside & bindingedge
         * (not feededge) */
        if ((pdev->LeadingEdge & 1) == 1) {  /* longedgefeed */
            if (pcs->back_side) {
                if (pcs->bind_short_edge) {
                    toff = -toff;
                } else {  /* bind_long_edge */
                    loff = -loff;
                }
            } else {
                /* do not change LOFF */
                /* TOFF ? */
            }
        } else {  /* shortedgefeed */
            if (pcs->back_side) {
                if (pcs->bind_short_edge) {
                    toff = -toff;
                } else {  /* bind_long_edge */
                    loff = -loff;
                }
            } else {
                /* do nothing */
            }
        }
    }

    pcl_make_rotation(pxfmst->lp_orient,
                      (double) (psize->width),
                      (double) (psize->height), &(pxfmst->lp2pg_mtx)
        );
    pxfmst->lp2pg_mtx.tx += loff;
    pxfmst->lp2pg_mtx.ty += toff;

    /* if RTL mode or there is PJL requesting full bleed the offsets
     * of the logical page are 0.
     */
    if ((pcs->personality == rtl) ||
        (!pjl_proc_compare(pcs->pjls,
                           pjl_proc_get_envvar(pcs->pjls, "edgetoedge"), "YES")))

        offset = 0;
    else
        offset = ((pxfmst->lp_orient & 0x1) != 0 ? psize->offset_landscape
                  : psize->offset_portrait);

    /* we need an extra 1/10 inch on each side to support 80
       characters vs. 78 at 10 cpi.  Only apply to A4. */
    if ((pcs->wide_a4) && (psize->width == 59520) && (psize->height == 84168))
        offset -= inch2coord(1.0 / 10.0);

    gs_matrix_translate(&(pxfmst->lp2pg_mtx),
                        (double) offset, 0.0, &(pxfmst->lp2pg_mtx)
        );
    if ((pxfmst->lp_orient & 0x1) != 0) {
        pxfmst->lp_size.x = psize->height - 2 * offset;
        pxfmst->lp_size.y = psize->width;
    } else {
        pxfmst->lp_size.x = psize->width - 2 * offset;
        pxfmst->lp_size.y = psize->height;
    }

    /* then the logical page to device transformation */
    gs_matrix_multiply(&(pxfmst->lp2pg_mtx), &pg2dev, &(pxfmst->lp2dev_mtx));
    pg2dev.ty = round(pg2dev.ty);
    pg2dev.tx = round(pg2dev.tx);
    pxfmst->lp2dev_mtx.tx = round(pxfmst->lp2dev_mtx.tx);
    pxfmst->lp2dev_mtx.ty = round(pxfmst->lp2dev_mtx.ty);
    /* the "pseudo page direction to logical page/device transformations */
    pcl_make_rotation(pxfmst->print_dir,
                      (double) pxfmst->lp_size.x,
                      (double) pxfmst->lp_size.y, &(pxfmst->pd2lp_mtx)
        );
    gs_matrix_multiply(&(pxfmst->pd2lp_mtx),
                       &(pxfmst->lp2dev_mtx), &(pxfmst->pd2dev_mtx)
        );

    /* calculate the print direction page size */
    if ((pxfmst->print_dir & 0x1) || (pcs->personality == rtl)) {
        pxfmst->pd_size.x = pxfmst->lp_size.y;
        pxfmst->pd_size.y = pxfmst->lp_size.x;
    } else
        pxfmst->pd_size = pxfmst->lp_size;

    {
        gx_device *pdev = gs_currentdevice(pcs->pgs);

        /* We must not set up a clipping region beyond the hardware
           margins of the device, but the pcl language definition
           requires hardware margins to be 1/6".  We set all margins
           to the the maximum of the PCL language defined 1/6" and the
           actual hardware margin.  If 1/6" is not available pcl will
           not work correctly all of the time.  The HPGL-2/RTL mode
           does not use margins. */
        if (pcs->personality == rtl) {
            print_rect.p.x = inch2coord(pdev->HWMargins[1]) / 72.0;
            print_rect.p.y = inch2coord(pdev->HWMargins[0] / 72.0);
            print_rect.q.x =
                psize->height - inch2coord(pdev->HWMargins[3] / 72.0);
            print_rect.q.y =
                psize->width - inch2coord(pdev->HWMargins[2] / 72.0);
        } else {
            print_rect.p.x =
                max(PRINTABLE_MARGIN_CP,
                    inch2coord(pdev->HWMargins[0] / 72.0));
            print_rect.p.y =
                max(PRINTABLE_MARGIN_CP,
                    inch2coord(pdev->HWMargins[1]) / 72.0);
            print_rect.q.x =
                psize->width - max(PRINTABLE_MARGIN_CP,
                                   inch2coord(pdev->HWMargins[2] / 72.0));
            print_rect.q.y =
                psize->height - max(PRINTABLE_MARGIN_CP,
                                    inch2coord(pdev->HWMargins[3] / 72.0));
        }
        pcl_transform_rect(&print_rect, &dev_rect, &pg2dev);
    }
    pcl_invert_mtx(&(pxfmst->lp2pg_mtx), &pg2lp);
    pcl_transform_rect(&print_rect, &(pxfmst->lp_print_rect), &pg2lp);

    /* restablish the current point and text region */
    if (!reset_initial)
        restore_cap_and_margins(pcs, &cur_pt, &text_rect);

    /*
     * No need to worry about pat_orient or pat_ref_pt; these will always
     * be recalculated just prior to use.
     */
}

/* default margins, relative to the logical page boundaries */
#define DFLT_TOP_MARGIN     (pcs->personality == rtl ? 0 : inch2coord(0.5))
#define DFLT_LEFT_MARGIN    inch2coord(0.0)
#define DFLT_RIGHT_MARGIN   inch2coord(0.0)
#define DFLT_BOTTOM_MARGIN  (pcs->personality == rtl ? 0 : inch2coord(0.5))

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
reset_vertical_margins(pcl_state_t * pcs, bool for_passthrough)
{
    pcl_margins_t *pmar = &(pcs->margins);
    coord hgt = pcs->xfm_state.pd_size.y;
    coord tm = (for_passthrough ?
                DFLT_TOP_MARGIN_PASSTHROUGH : DFLT_TOP_MARGIN);
    coord bm = (for_passthrough ?
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
reset_horizontal_margins(pcl_state_t * pcs)
{
    pcl_margins_t *pmar = &(pcs->margins);

    pmar->left = DFLT_LEFT_MARGIN;
    pmar->right = pcs->xfm_state.pd_size.x - DFLT_RIGHT_MARGIN;
}

/*
 * Reset both the horizontal and vertical margins
 */
static void
reset_margins(pcl_state_t * pcs, bool for_passthrough)
{
    reset_horizontal_margins(pcs);
    reset_vertical_margins(pcs, for_passthrough);
}

static void
reset_default_transformation(pcl_state_t *pcs)
{
    gs_gstate *pgs = pcs->pgs;
    gs_matrix mat;
    double height_pts = pcs->xfm_state.paper_size->height * 0.01;

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

    if (pcs->personality != rtl) {
        gs_matrix_translate(&mat, 0.0, height_pts, &mat);
        gs_matrix_scale(&mat, 0.01, -0.01, &mat);
    } else {
        gs_matrix_rotate(&mat, -90, &mat);
        gs_matrix_scale(&mat, -0.01, 0.01, &mat);
    }

    gs_setdefaultmatrix(pgs, &mat);
}

/*
 * Reset all parameters which must be reset whenever the page size changes.
 *
 * The third operand indicates if this routine is being called as part of
 * an initial reset. In that case, done't call HPGL's reset - the reset
 * will do that later.
 */
static int
new_page_size(pcl_state_t * pcs,
              const pcl_paper_size_t * psize,
              bool reset_initial, bool for_passthrough)
{
    double width_pts = psize->width * 0.01;
    double height_pts = psize->height * 0.01;
    float page_size[2];
    float old_page_size[2];
    bool changed_page_size;
    int code = 0;

    page_size[0] = width_pts;
    page_size[1] = height_pts;

    old_page_size[0] = gs_currentdevice(pcs->pgs)->MediaSize[0];
    old_page_size[1] = gs_currentdevice(pcs->pgs)->MediaSize[1];

    code = put_param1_float_array(pcs, "PageSize", page_size);
    if (code < 0) return code;

    pcs->xfm_state.paper_size = psize;
    reset_default_transformation(pcs);
    pcs->overlay_enabled = false;
    update_xfm_state(pcs, reset_initial);
    reset_margins(pcs, for_passthrough);

    /* check if update_xfm_state changed the page size */
    changed_page_size = !((int)old_page_size[0] == pcs->xfm_state.paper_size->width/100 &&
                          (int)old_page_size[1] == pcs->xfm_state.paper_size->height/100);

    /*
     * make sure underlining is disabled (homing the cursor may cause
     * an underline to be put out.
     */
    pcs->underline_enabled = false;
    code = pcl_home_cursor(pcs);
    if (code < 0) return code;
    /*
     * this is were we initialized the cursor position
     */
    pcs->cursor_moved = false;

    pcl_xfm_reset_pcl_pat_ref_pt(pcs);

    if (!reset_initial)
        code = hpgl_do_reset(pcs, pcl_reset_page_params);
    if (code < 0) return code;

    if (pcs->end_page == pcl_end_page_top) {    /* don't erase in snippet mode */
        if (pcs->page_marked || changed_page_size) {
            code = gs_erasepage(pcs->pgs);
            pcs->page_marked = false;
        }
    }

    return code;
}

/*
 * Define the known paper sizes.
 *
 * The values are taken from the H-P manual and are in 1/300" units,
 * but the structure values are in centipoints (1/7200").
 */
#define p_size(t, n, w, h, offp, offl)                                  \
    { (t), (n), { (w) * 24L, (h) * 24L, (offp) * 24L, (offl) * 24L } }

pcl_paper_type_t paper_types_proto[] = {
    p_size(1, "executive", 2175, 3150, 75, 60),
    p_size(2, "letter", 2550, 3300, 75, 60),
    p_size(3, "legal", 2550, 4200, 75, 60),
    p_size(6, "ledger", 3300, 5100, 75, 60),
    p_size(26, "a4", 2480, 3507, 71, 59),
    p_size(27, "a3", 3507, 4960, 71, 59),
    p_size(78, "index_3x5", 900, 1500, 75, 60),
    p_size(80, "monarch", 1162, 2250, 75, 60),
    p_size(81, "com_10", 1237, 2850, 75, 60),
    p_size(90, "dl", 1299, 2598, 71, 59),
    p_size(91, "c5", 1913, 2704, 71, 59),
    p_size(100, "b5", 2078, 2952, 71, 59),
    /* initial values for custom: copied from default(letter) */
    /* offset_portrait/offset_landscape ? */
    p_size(101, "custom", 2550, 3300, 75, 60)
};

const int pcl_paper_type_count = countof(paper_types_proto);

/*
 * Reset all parameters which must be reset whenever the logical page
 * orientation changes.
 *
 * The last operand indicates if this routine is being called as part of
 * an initial resete.
 */
int
new_logical_page(pcl_state_t * pcs,
                 int lp_orient,
                 const pcl_paper_size_t * psize,
                 bool reset_initial, bool for_passthrough)
{
    pcl_xfm_state_t *pxfmst = &(pcs->xfm_state);

    pxfmst->lp_orient = lp_orient;
    pxfmst->print_dir = 0;
    return new_page_size(pcs, psize, reset_initial, for_passthrough);
}


#define PAPER_SIZES pcs->ppaper_type_table

int
pcl_new_logical_page_for_passthrough(pcl_state_t * pcs, int orient,
                                     gs_point * pdims)
{
    int i;
    pcl_paper_size_t *psize;
    /* points to centipoints */
    coord cp_width = (coord) (pdims->x * 100 + 0.5);
    coord cp_height = (coord) (pdims->y * 100 + 0.5);
    bool found = false;

    for (i = 0; i < pcl_paper_type_count; i++) {
        psize = &(PAPER_SIZES[i].psize);
        if (psize->width == cp_width && psize->height == cp_height) {
            found = true;
            break;
        }
    }

    if (!found)
        return -1;
    return new_logical_page(pcs, orient, psize, false, true);
}

/* page marking routines */


int
pcl_mark_page_for_current_pos(pcl_state_t * pcs)
{
	int code = 0;

    /* nothing to do */
    if (pcs->page_marked)
        return code;

    /* convert current point to device space and check if it is inside
       device rectangle for the page */
    {
        gs_fixed_rect page_box;
        gs_fixed_point pt;

        code = gx_default_clip_box(pcs->pgs, &page_box);
        if (code < 0)
            /* shouldn't happen. */
            return code;

        code = gx_path_current_point(gx_current_path(pcs->pgs), &pt);
        if (code < 0)
            /* shouldn't happen */
            return code;


        /* half-open lower - not sure this is correct */
        if (pt.x >= page_box.p.x && pt.y >= page_box.p.y &&
            pt.x < page_box.q.x && pt.y < page_box.q.y)
            pcs->page_marked = true;
    }
    return code;
}


int
pcl_mark_page_for_character(pcl_state_t * pcs, gs_fixed_point *org)
{
    /* nothing to do */
    if (pcs->page_marked)
        return 0;

    /* convert current point to device space and check if it is inside
       device rectangle for the page */
    {
        gs_fixed_rect page_box;
        gs_fixed_point pt;
        int code;

        code = gx_default_clip_box(pcs->pgs, &page_box);
        if (code < 0)
            /* shouldn't happen. */
            return code;

        code = gx_path_current_point(gx_current_path(pcs->pgs), &pt);
        if (code < 0)
            /* shouldn't happen */
            return code;


        /* half-open lower - not sure this is correct */
        if (pt.x >= page_box.p.x && pt.y >= page_box.p.y &&
            org->x < page_box.q.x && org->y < page_box.q.y)
            pcs->page_marked = true;
    }
    return 0;
}

/*
 *  Just use the current position to see if we marked the page.  This
 *  could be improved by using the bounding box of the path object but
 *  for page marking that case does not seem to come up in practice.
 */
int
pcl_mark_page_for_path(pcl_state_t * pcs)
{
    return pcl_mark_page_for_current_pos(pcs);
}

/* returns the bounding box coordinates for the current device and a
   boolean to indicate marked status. 0 - unmarked 1 - marked -1 error */
int
pcl_page_marked(pcl_state_t * pcs)
{
    return pcs->page_marked;
}

int
pcl_cursor_moved(pcl_state_t * pcs)
{
    return pcs->cursor_moved;
}

/*
 * End a page, either unconditionally or only if there are marks on it.
 * Return 1 if the page was actually printed and erased.
 */
int
pcl_end_page(pcl_state_t * pcs, pcl_print_condition_t condition)
{
    int code = pcl_break_underline(pcs);   /* (could mark page) */
    if (code < 0)
        return code;

    /* If we are conditionally printing (normal case) check if the
       page is marked */
    if (condition != pcl_print_always) {
        if (!pcl_page_marked(pcs))
            return 0;
    }

    /* finish up graphics mode in case we finished the page in the
       middle of a raster stream */
    if (pcs->raster_state.graphics_mode)
        code = pcl_end_graphics_mode(pcs);
    if (code < 0)
        return code;

    /* If there's an overlay macro, execute it now. */
    if (pcs->overlay_enabled) {
        void *value;

        if (pl_dict_find(&pcs->macros,
                         id_key(pcs->overlay_macro_id), 2, &value)) {
            pcs->overlay_enabled = false;   /**** IN reset_overlay ****/
            code = pcl_execute_macro((const pcl_macro_t *)value,
                                     pcs,
                                     pcl_copy_before_overlay,
                                     pcl_reset_overlay,
                                     pcl_copy_after_overlay);
            if (code < 0)
                return code;

            pcs->overlay_enabled = true; /**** IN copy_after ****/
        }
    }
    /* output the page */
    code = (*pcs->end_page) (pcs, pcs->num_copies, true);
    if (code < 0)
        return code;

    if (pcs->end_page == pcl_end_page_top)
        code = gs_erasepage(pcs->pgs);
    if (code < 0)
        return code;

    pcs->page_marked = false;

    /*
     * Advance of a page may move from a page front to a page back. This may
     * change the applicable transformations.
     */
    /*
     * Keep track of the side you are on
     */
    if (pcs->duplex) {
        pcs->back_side = ! pcs->back_side;
    } else {
        pcs->back_side = false;
    }
    code = put_param1_bool(pcs,"FirstSide", !pcs->back_side);
    reset_default_transformation(pcs);
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
set_page_size(pcl_args_t * pargs, pcl_state_t * pcs)
{
    /* Note: not all values are implemented on all printers.  If -g
       has been given on the command line we use the "Custom" page
       tag, the height and width are set to the device media values
       which were filled in when the -g option was processed. */

    uint tag = (pcs->page_set_on_command_line ? 101 : uint_arg(pargs));
    int i;
    int code = 0;
    const pcl_paper_size_t *psize = 0;

    /* oddly the command goes to the next page irrespective of
       arguments */
    code = pcl_end_page_if_marked(pcs);
    if (code < 0)
        return code;
    code = pcl_home_cursor(pcs);
    if (code < 0)
        return code;

    for (i = 0; i < pcl_paper_type_count; i++) {
        if (tag == PAPER_SIZES[i].tag) {
            psize = &(PAPER_SIZES[i].psize);
            break;
        }
    }
    if ((psize != 0) && ((code = pcl_end_page_if_marked(pcs)) >= 0)) {
        pcs->xfm_state.print_dir = 0;
        code = new_page_size(pcs, psize, false, false);
    }
    return code;
}

/*
 * ESC & l <feed_enum> H
 *
 * Set paper source
 */
static int
set_paper_source(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);
    /* oddly the command goes to the next page irrespective of
       arguments */
    int code = pcl_end_page_if_marked(pcs);

    if (code < 0)
        return code;
    code = pcl_home_cursor(pcs);
    if (code < 0)
        return code;
    /* Do not change the page side if the wanted paper source is the same as the actual one */
    if (pcs->paper_source != i) {
        pcs->back_side = false;
        code = put_param1_bool(pcs, "FirstSide", !pcs->back_side);
        if (code < 0)
            return code;
    }
    pcs->paper_source = i;
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
set_left_offset_registration(pcl_args_t * pargs, pcl_state_t * pcs)
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
set_top_offset_registration(pcl_args_t * pargs, pcl_state_t * pcs)
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
set_logical_page_orientation(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);
    int code;
    /* the command is ignored if it is value is out of range */
    if (i > 3)
        return 0;

    /* this command is ignored in pcl xl snippet mode.  NB we need a
       better flag for snippet mode. */
    if (pcs->end_page != pcl_end_page_top) {
        return 0;
    }

    /* If orientation is same as before ignore the command */
    if (i == pcs->xfm_state.lp_orient) {
        return 0;
    }

    /* ok to execute - clear the page, set up the transformations and
       set the flag disabling the orientation command for this page. */
    code = pcl_end_page_if_marked(pcs);
    if (code >= 0) {
        /* a page_orientation change is not the same as a new page.
           If a cursor has moved this should be remembered */
        bool cursor_moved = pcs->cursor_moved;
        code = new_logical_page(pcs, i, pcs->xfm_state.paper_size, false, false);
        pcs->cursor_moved = cursor_moved;
        pcs->hmi_cp = HMI_DEFAULT;
        pcs->vmi_cp = VMI_DEFAULT;
    }
    return code;
}

/*
 * ESC & a <angle> P
 */
static int
set_print_direction(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint i = uint_arg(pargs);

    if ((i <= 270) && (i % 90 == 0)) {
        i /= 90;
        if (i != pcs->xfm_state.print_dir) {
            int code = pcl_break_underline(pcs);
            if (code < 0)
                return code;
            pcs->xfm_state.print_dir = i;
            update_xfm_state(pcs, 0);
            pcl_continue_underline(pcs);
        } else {
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
set_left_margin(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code = pcl_update_hmi_cp(pcs);
    coord lmarg = uint_arg(pargs) * pcs->hmi_cp;

    if (code < 0)
        return code;

    /* adjust underlining if the left margin passes to the right of
       the underline start position */
    if ((pcs->underline_enabled) && (lmarg > pcs->underline_start.x))
        pcs->underline_start.x = lmarg;

    if (lmarg < pcs->margins.right) {
        pcs->margins.left = lmarg;
        if (pcs->cap.x < lmarg)
            code = pcl_set_cap_x(pcs, lmarg, false, false);
    }
    return code;
}

/*
 * ESC & a <col> M
 *
 * Set right margin. The right margin is set to the *right* edge of the
 * specified column, so we need to add 1 to the column number.
 */
static int
set_right_margin(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int code = pcl_update_hmi_cp(pcs);
    coord rmarg = (uint_arg(pargs) + 1) * pcs->hmi_cp;

    if (code < 0)
        return code;

    if (rmarg > pcs->xfm_state.pd_size.x)
        rmarg = pcs->xfm_state.pd_size.x;
    if (rmarg > pcs->margins.left) {
        pcs->margins.right = rmarg;
        if (pcs->cap.x > rmarg)
            code = pcl_set_cap_x(pcs, rmarg, false, false);
    }

    return code;
}

/*
 * ESC 9
 *
 * Clear horizontal margins.
 */
static int
clear_horizontal_margins(pcl_args_t * pargs,    /* ignored */
                         pcl_state_t * pcs)
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
set_top_margin(pcl_args_t * pargs, pcl_state_t * pcs)
{
    coord hgt = pcs->xfm_state.pd_size.y;
    coord tmarg = uint_arg(pargs) * pcs->vmi_cp;

    if ((pcs->vmi_cp != 0) && (tmarg <= hgt)) {
        pcs->margins.top = tmarg;
        pcs->margins.length = PAGE_LENGTH(hgt - tmarg, DFLT_BOTTOM_MARGIN);
        /* See the discussion in the Implementor's guide concerning
           "fixed" and "floating" cap.  If the page is not dirty we
           home the cursor - the guide talks about tracking the cursor
           for any movement and checking for data on the page */
        if (!pcl_page_marked(pcs) && !pcl_cursor_moved(pcs))
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
set_text_length(pcl_args_t * pargs, pcl_state_t * pcs)
{
    coord len = uint_arg(pargs) * pcs->vmi_cp;

    if (len == 0) {
        len =
            pcs->xfm_state.pd_size.y - pcs->margins.top -
            inch2coord(1.0 / 2.0);
        if (len < 0) {
            len += inch2coord(1.0 / 2.0);
        }
    }
    if ((len >= 0) && (pcs->margins.top + len <= pcs->xfm_state.pd_size.y))
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
set_perforation_skip(pcl_args_t * pargs, pcl_state_t * pcs)
{
    bool new_skip = uint_arg(pargs);

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
pcl_media_type(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int type = uint_arg(pargs);

    if (type <= 4) {
        int code = pcl_end_page_if_marked(pcs);

        if (code >= 0)
            code = pcl_home_cursor(pcs);
        return (code < 0 ? code : e_Unimplemented);
    } else
        return e_Range;
}

/*
 * Logical Page Setup (From HP D640 Technical Manual)
 */

typedef struct pcl_logical_page_s
{
    byte LeftOffset[2];
    byte TopOffset[2];
    byte Orientation;
    byte Reserved;
    byte Width[2];
    byte Height[2];
} pcl_logical_page_t;

/*
 * ESC & a # W
 *
 * Set logical page.
 */

static int
set_logical_page(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint count = uint_arg(pargs);
    int code = 0;

    const pcl_logical_page_t *plogpage =
        (pcl_logical_page_t *) arg_data(pargs);
    pcl_paper_size_t *pcur_paper;

#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_debug_dump_data(pcs->memory, arg_data(pargs), uint_arg(pargs));
    }
#endif

    /* the currently selected paper size */
    pcur_paper = (pcl_paper_size_t *) pcs->xfm_state.paper_size;

    /* the command can set width, height and offsets (10) or just
       offsets (4) */
    if (count != 10 && count != 4)
        return e_Range;

    if (count == 10) {
        pcur_paper->width = pl_get_uint16(plogpage->Width) * 10;
        pcur_paper->height = pl_get_uint16(plogpage->Height) * 10;
        if (pcur_paper->width == 0 || pcur_paper->height == 0)
            return e_Range;
    }

    pcur_paper->offset_portrait = pl_get_int16(plogpage->LeftOffset) * 10;
    pcur_paper->offset_landscape = pl_get_int16(plogpage->TopOffset) * 10;

    code = new_page_size(pcs, pcur_paper, false, false);
    if (code < 0) return code;

    code = gs_erasepage(pcs->pgs);
    pcs->page_marked = false;
    return code;
}

/*
 * Custom Paper Width/Length
 * from Windows Driver "HP Universal Printing PCL 5" (.GPD files) and
 * http://www.office.xerox.com/support/dctips/dc10cc0471.pdf
 *
 * ESC & f <decipoints> I
 *
 */
static int
set_paper_width(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint decipoints = uint_arg(pargs);
    bool found = false;
    int i;
    pcl_paper_size_t *psize;

    for (i = 0; i < pcl_paper_type_count; i++) {
        if (101 == PAPER_SIZES[i].tag) {
            psize = &(PAPER_SIZES[i].psize);
            found = true;
            break;
        }
    }

    if (found)
        psize->width = decipoints * 10L;
    else
        gs_warn("Page table does not contain a custom entry");

    return 0;
}

int
pcl_set_custom_paper_size(pcl_state_t *pcs, pcl_paper_size_t *p)
{
    pcl_paper_size_t *psize = NULL;
    bool found = false;
    int i;

    for (i = 0; i < pcl_paper_type_count; i++) {
        if (101 == PAPER_SIZES[i].tag) {
            psize = &(PAPER_SIZES[i].psize);
            found = true;
            break;
        }
    }
    if (found)
        *psize = *p;
    else
        /* this should never happen - the custom paper size is
           always in the table */
        return -1;

    return new_logical_page(pcs, 0, psize, false, false);
}
/*
 * ESC & f <decipoints> J
 */

static int
set_paper_length(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint decipoints = uint_arg(pargs);
    bool found = false;
    int i;
    pcl_paper_size_t *psize;

    for (i = 0; i < pcl_paper_type_count; i++) {
        if (101 == PAPER_SIZES[i].tag) {
            psize = &(PAPER_SIZES[i].psize);
            found = true;
            break;
        }
    }
    if (found)
        psize->height = decipoints * 10L;
    else
        /* never happens the, custom paper size is always in the
           table */
        return -1;
    return 0;
}

/*
 * (From PCL5 Comparison Guide, p. 1-99)
 *
 * ESC * o <quality> Q
 *
 * Set print quality.
 *
 * We don't implement this command, it appears to only be supported on
 * DeskJet 1200C, an obsolete inkjet, which is not a target of
 * emulation.
 */

static int
pcl_print_quality(pcl_args_t * pargs, pcl_state_t * pcs)
{

    return_error(e_Unimplemented);
}

/*
 * Initialization
 */
static int
pcpage_do_registration(pcl_parser_state_t * pcl_parser_state, gs_memory_t * pmem        /* ignored */
    )
{
    /* Register commands */
    DEFINE_CLASS('&') {
        'l', 'A',
            PCL_COMMAND("Page Size",
                        set_page_size, pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'H',
            PCL_COMMAND("Paper Source",
                        set_paper_source, pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'U',
            PCL_COMMAND("Left Offset Registration",
                        set_left_offset_registration,
                        pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'Z',
            PCL_COMMAND("Top Offset Registration",
                        set_top_offset_registration,
                        pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'O',
            PCL_COMMAND("Page Orientation",
                        set_logical_page_orientation,
                        pca_neg_ok | pca_big_ignore)
    }, {
        'a', 'P',
            PCL_COMMAND("Print Direction",
                        set_print_direction, pca_neg_ok | pca_big_ignore)
    }, {
        'a', 'L',
            PCL_COMMAND("Left Margin",
                        set_left_margin, pca_neg_ok | pca_big_ignore)
    }, {
        'a', 'M',
            PCL_COMMAND("Right Margin",
                        set_right_margin, pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'E',
            PCL_COMMAND("Top Margin",
                        set_top_margin, pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'F',
            PCL_COMMAND("Text Length",
                        set_text_length, pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'L',
            PCL_COMMAND("Perforation Skip",
                        set_perforation_skip, pca_neg_ok | pca_big_ignore)
    }, {
        'l', 'M',
            PCL_COMMAND("Media Type",
                        pcl_media_type, pca_neg_ok | pca_big_ignore)
    }, {
        'a', 'W', PCL_COMMAND("Set Logical Page", set_logical_page, pca_bytes)
    }, END_CLASS DEFINE_CLASS('&') {
        'f', 'I',
            PCL_COMMAND("Set Custom Paper Width",
                        set_paper_width, pca_neg_ok | pca_big_ignore)
    }, {
        'f', 'J',
            PCL_COMMAND("Set Custom Paper Length",
                        set_paper_length, pca_neg_ok | pca_big_ignore)
    }, END_CLASS
        DEFINE_ESCAPE('9',
                      "Clear Horizontal Margins", clear_horizontal_margins)

        DEFINE_CLASS_COMMAND_ARGS('*',
                                  'o',
                                  'Q',
                                  "Print Quality",
                                  pcl_print_quality,
                                  pca_neg_ok | pca_big_ignore)
        return 0;
}

pcl_paper_size_t *
pcl_get_default_paper(pcl_state_t * pcs)
{
    int i;
    pjl_envvar_t *pwidth = pjl_proc_get_envvar(pcs->pjls, "paperwidth");
    pjl_envvar_t *plength = pjl_proc_get_envvar(pcs->pjls, "paperlength");
    pjl_envvar_t *psize = pjl_proc_get_envvar(pcs->pjls, "paper");

    /* build the state's paper table if it doesn't exist */
    if (!pcs->ppaper_type_table)
        pcs->ppaper_type_table =
            (pcl_paper_type_t *) gs_alloc_bytes(pcs->memory,
                                                sizeof(paper_types_proto),
                                                "Paper Table");

    if (!pcs->ppaper_type_table)
        /* should never fail */
        return NULL;

    /* restore default table values - this would only reset custom paper sizes */
    memcpy(pcs->ppaper_type_table, paper_types_proto,
           sizeof(paper_types_proto));

    pcs->wide_a4 = false;
    if (pcs->page_set_on_command_line || (*pwidth && *plength)) {
        for (i = 0; i < pcl_paper_type_count; i++)
            if (!pjl_proc_compare(pcs->pjls, "custom", PAPER_SIZES[i].pname)) {
                if (pcs->page_set_on_command_line) {
                    PAPER_SIZES[i].psize.width =
                        (coord)(gs_currentdevice(pcs->pgs)->MediaSize[0] * 100);
                    PAPER_SIZES[i].psize.height =
                        (coord)(gs_currentdevice(pcs->pgs)->MediaSize[1] * 100);
                } else {
                    PAPER_SIZES[i].psize.width = atol(pwidth) * 10L;
                    PAPER_SIZES[i].psize.height = atol(plength) * 10L;
                }
                /* just a guess, values copied from letter entry in
                   table PAPER_SIZES. NB offsets should be 0 for
                   RTL. */
                PAPER_SIZES[i].psize.offset_portrait = 75 * 24L;
                PAPER_SIZES[i].psize.offset_landscape = 60 * 24L;
                return &(PAPER_SIZES[i].psize);
            }
    }

    for (i = 0; i < pcl_paper_type_count; i++)
        if (!pjl_proc_compare(pcs->pjls, psize, PAPER_SIZES[i].pname)) {
            /* set wide a4, only used if the paper is a4 */
            if (!pjl_proc_compare
                (pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "widea4"), "YES"))
                pcs->wide_a4 = true;
            return &(PAPER_SIZES[i].psize);
        }
    dmprintf(pcs->memory,
             "system does not support requested paper setting\n");
    return &(PAPER_SIZES[1].psize);
}

static int
pcpage_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
	int code = 0;

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
        return 0;
    }

    if ((type & (pcl_reset_initial | pcl_reset_printer)) != 0) {
        pcl_paper_size_t *psize = pcl_get_default_paper(pcs);
        if (psize == NULL) return_error(gs_error_VMerror);

        pcs->paper_source = 0;  /* ??? */
        pcs->xfm_state.left_offset_cp = 0.0;
        pcs->xfm_state.top_offset_cp = 0.0;
        pcs->perforation_skip = 1;
        code = new_logical_page(pcs,
                         !pjl_proc_compare(pcs->pjls,
                                           pjl_proc_get_envvar(pcs->pjls,
                                                               "orientation"),
                                           "portrait") ? 0 : 1,
                         psize,
                         (type & pcl_reset_initial) != 0, false);
        if (code < 0) goto cleanup;
    } else if ((type & pcl_reset_overlay) != 0) {
        pcs->perforation_skip = 1;
        update_xfm_state(pcs, 0);
        reset_margins(pcs, false);
        pcl_xfm_reset_pcl_pat_ref_pt(pcs);
    } else if ((type & pcl_reset_permanent) != 0) {
        goto cleanup;
    }
    return 0;

cleanup:
    if (pcs->ppaper_type_table) {
        gs_free_object(pcs->memory, pcs->ppaper_type_table, "Paper Table");
        pcs->ppaper_type_table = 0;
    }
    return code;
}

const pcl_init_t pcpage_init = { pcpage_do_registration, pcpage_do_reset, 0 };
