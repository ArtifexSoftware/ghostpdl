/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Display PostScript graphics additions for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"
#include "gspaint.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxhldevc.h"
#include "gspath.h"
#include "gspath2.h"		/* defines interface */
#include "gzpath.h"
#include "gzcpath.h"
#include "gzstate.h"
#include "gsutil.h"
#include "gxdevsop.h"

/*
 * Define how much rounding slop setbbox should leave,
 * in device coordinates.  Because of rounding in transforming
 * path coordinates to fixed point, the minimum realistic value is:
 *
 *      #define box_rounding_slop_fixed (fixed_epsilon)
 *
 * But even this isn't enough to compensate for cumulative rounding error
 * in rmoveto or rcurveto.  Instead, we somewhat arbitrarily use:
 */
#define box_rounding_slop_fixed (fixed_epsilon * 3)

/* ------ Graphics state ------ */

/* Set the bounding box for the current path. */
int
gs_setbbox(gs_gstate * pgs, double llx, double lly, double urx, double ury)
{
    gs_rect ubox, dbox;
    gs_fixed_rect obox, bbox;
    gx_path *ppath = pgs->path;
    int code;

    if (llx > urx || lly > ury)
        return_error(gs_error_rangecheck);
    /* Transform box to device coordinates. */
    ubox.p.x = llx;
    ubox.p.y = lly;
    ubox.q.x = urx;
    ubox.q.y = ury;
    if ((code = gs_bbox_transform(&ubox, &ctm_only(pgs), &dbox)) < 0)
        return code;
    /* Round the corners in opposite directions. */
    /* Because we can't predict the magnitude of the dbox values, */
    /* we add/subtract the slop after fixing. */
    if (dbox.p.x < fixed2float(min_fixed + box_rounding_slop_fixed) ||
        dbox.p.y < fixed2float(min_fixed + box_rounding_slop_fixed) ||
        dbox.q.x >= fixed2float(max_fixed - box_rounding_slop_fixed + fixed_epsilon) ||
        dbox.q.y >= fixed2float(max_fixed - box_rounding_slop_fixed + fixed_epsilon)
        )
        return_error(gs_error_limitcheck);
    bbox.p.x =
        (fixed) floor(dbox.p.x * fixed_scale) - box_rounding_slop_fixed;
    bbox.p.y =
        (fixed) floor(dbox.p.y * fixed_scale) - box_rounding_slop_fixed;
    bbox.q.x =
        (fixed) ceil(dbox.q.x * fixed_scale) + box_rounding_slop_fixed;
    bbox.q.y =
        (fixed) ceil(dbox.q.y * fixed_scale) + box_rounding_slop_fixed;
    if (gx_path_bbox_set(ppath, &obox) >= 0) {	/* Take the union of the bboxes. */
        ppath->bbox.p.x = min(obox.p.x, bbox.p.x);
        ppath->bbox.p.y = min(obox.p.y, bbox.p.y);
        ppath->bbox.q.x = max(obox.q.x, bbox.q.x);
        ppath->bbox.q.y = max(obox.q.y, bbox.q.y);
    } else {			/* empty path *//* Just set the bbox. */
        ppath->bbox = bbox;
    }
    ppath->bbox_set = 1;
    return 0;
}

/* ------ Rectangles ------ */

/* Append a list of rectangles to a path. */
static int
gs_rectappend_compat(gs_gstate * pgs, const gs_rect * pr, uint count, bool clip)
{
    bool CPSI_mode = gs_currentcpsimode(pgs->memory);

    for (; count != 0; count--, pr++) {
        double px = pr->p.x, py = pr->p.y, qx = pr->q.x, qy = pr->q.y;
        int code;

        if (CPSI_mode) {
            /* We believe that the result must be independent
               on the device initial matrix.
               Particularly for the correct dashing
               the starting point and the contour direction
               must be same with any device initial matrix.
               Only way to provide it is to choose the starting point
               and the direction in the user space. */
            if (clip) {
                /* CPSI starts a clippath with the upper right corner. */
                /* Debugged with CET 11-11.PS page 6 item much13.*/
                if ((code = gs_moveto(pgs, qx, qy)) < 0 ||
                    (code = gs_lineto(pgs, qx, py)) < 0 ||
                    (code = gs_lineto(pgs, px, py)) < 0 ||
                    (code = gs_lineto(pgs, px, qy)) < 0 ||
                    (code = gs_closepath(pgs)) < 0
                    )
                    return code;
            } else {
                /* Debugged with CET 12-12.PS page 10 item more20.*/
                if (px > qx) {
                    px = qx; qx = pr->p.x;
                }
                if (py > qy) {
                    py = qy; qy = pr->p.y;
                }
                if ((code = gs_moveto(pgs, px, py)) < 0 ||
                    (code = gs_lineto(pgs, qx, py)) < 0 ||
                    (code = gs_lineto(pgs, qx, qy)) < 0 ||
                    (code = gs_lineto(pgs, px, qy)) < 0 ||
                    (code = gs_closepath(pgs)) < 0
                    )
                    return code;
            }
        } else {
            /* Ensure counter-clockwise drawing. */
            if ((qx >= px) != (qy >= py))
                qx = px, px = pr->q.x;	/* swap x values */
            if ((code = gs_moveto(pgs, px, py)) < 0 ||
                (code = gs_lineto(pgs, qx, py)) < 0 ||
                (code = gs_lineto(pgs, qx, qy)) < 0 ||
                (code = gs_lineto(pgs, px, qy)) < 0 ||
                (code = gs_closepath(pgs)) < 0
                )
                return code;
        }
    }
    return 0;
}
int
gs_rectappend(gs_gstate * pgs, const gs_rect * pr, uint count)
{
    return gs_rectappend_compat(pgs, pr, count, false);
}

/* Clip to a list of rectangles. */
int
gs_rectclip(gs_gstate * pgs, const gs_rect * pr, uint count)
{
    int code;
    gx_path save;

    gx_path_init_local(&save, pgs->memory);
    gx_path_assign_preserve(&save, pgs->path);
    gs_newpath(pgs);
    if ((code = gs_rectappend_compat(pgs, pr, count, true)) < 0 ||
        (code = gs_clip(pgs)) < 0
        ) {
        gx_path_assign_free(pgs->path, &save);
        return code;
    }
    gx_path_free(&save, "gs_rectclip");
    gs_newpath(pgs);
    return 0;
}

/* Setup for black vector handling */
static inline bool black_vectors(gs_gstate *pgs, gx_device *dev)
{
    if (dev->icc_struct != NULL && dev->icc_struct->blackvector &&
        pgs->black_textvec_state == NULL) {
        return gsicc_setup_blacktextvec(pgs, dev, false);
    }
    return false;
}

/* Fill a list of rectangles. */
/* We take the trouble to do this efficiently in the simple cases. */
int
gs_rectfill(gs_gstate * pgs, const gs_rect * pr, uint count)
{
    const gs_rect *rlist = pr;
    gx_clip_path *pcpath;
    uint rcount = count;
    int code;
    gx_device * pdev = pgs->device;
    gx_device_color *pdc = gs_currentdevicecolor_inline(pgs);
    const gs_gstate *pgs2 = (const gs_gstate *)pgs;
    bool hl_color_available = gx_hld_is_hl_color_available(pgs2, pdc);
    bool hl_color = (hl_color_available &&
                dev_proc(pdev, dev_spec_op)(pdev, gxdso_supports_hlcolor,
                                  NULL, 0));
    bool center_of_pixel = (pgs->fill_adjust.x == 0 && pgs->fill_adjust.y == 0);
    bool black_vector = false;

    /* Processing a fill object operation */
    ensure_tag_is_set(pgs, pgs->device, GS_VECTOR_TAG);	/* NB: may unset_dev_color */

    black_vector = black_vectors(pgs, pgs->device); /* Set vector fill to black */

    code = gx_set_dev_color(pgs);
    if (code != 0)
        goto exit;

    if ( !(pgs->device->page_uses_transparency ||
          dev_proc(pgs->device, dev_spec_op)(pgs->device,
              gxdso_is_pdf14_device, &(pgs->device),
              sizeof(pgs->device))) &&
        (is_fzero2(pgs->ctm.xy, pgs->ctm.yx) ||
         is_fzero2(pgs->ctm.xx, pgs->ctm.yy)) &&
        gx_effective_clip_path(pgs, &pcpath) >= 0 &&
        clip_list_is_rectangle(gx_cpath_list(pcpath)) &&
        (hl_color ||
         pdc->type == gx_dc_type_pure ||
         pdc->type == gx_dc_type_ht_binary ||
         pdc->type == gx_dc_type_ht_colored) &&
        gs_gstate_color_load(pgs) >= 0 &&
        (*dev_proc(pdev, get_alpha_bits)) (pdev, go_graphics)
        <= 1 &&
        (!pgs->overprint || !gs_currentcolor_eopm(pgs))
        ) {
        uint i;
        gs_fixed_rect clip_rect;

        gx_cpath_inner_box(pcpath, &clip_rect);
        /* We should never plot anything for an empty clip rectangle */
        if ((clip_rect.p.x >= clip_rect.q.x) &&
            (clip_rect.p.y >= clip_rect.q.y))
            goto exit;
        for (i = 0; i < count; ++i) {
            gs_fixed_point p, q;
            gs_fixed_rect draw_rect;

            if (gs_point_transform2fixed(&pgs->ctm, pr[i].p.x, pr[i].p.y, &p) < 0 ||
                gs_point_transform2fixed(&pgs->ctm, pr[i].q.x, pr[i].q.y, &q) < 0
                ) {		/* Switch to the slow algorithm. */
                goto slow;
            }
            draw_rect.p.x = min(p.x, q.x);
            draw_rect.p.y = min(p.y, q.y);
            draw_rect.q.x = max(p.x, q.x);
            draw_rect.q.y = max(p.y, q.y);
            if (hl_color) {
                rect_intersect(draw_rect, clip_rect);
                /* We do pass on 0 extant rectangles to high level
                   devices.  It isn't clear how a client and an output
                   device should interact if one uses a center of
                   pixel algorithm and the other uses any part of
                   pixel.  For now we punt and just pass the high
                   level rectangle on without adjustment. */
                if (draw_rect.p.x <= draw_rect.q.x &&
                    draw_rect.p.y <= draw_rect.q.y) {
                    code = dev_proc(pdev, fill_rectangle_hl_color)(pdev,
                             &draw_rect, pgs2, pdc, pcpath);
                    if (code < 0)
                        goto exit;
                }
            } else {
                int x, y, w, h;

                rect_intersect(draw_rect, clip_rect);
                if (center_of_pixel) {
                    draw_rect.p.x = fixed_rounded(draw_rect.p.x);
                    draw_rect.p.y = fixed_rounded(draw_rect.p.y);
                    draw_rect.q.x = fixed_rounded(draw_rect.q.x);
                    draw_rect.q.y = fixed_rounded(draw_rect.q.y);
                } else { /* any part of pixel rule - touched */
                    draw_rect.p.x = fixed_floor(draw_rect.p.x);
                    draw_rect.p.y = fixed_floor(draw_rect.p.y);
                    draw_rect.q.x = fixed_ceiling(draw_rect.q.x);
                    draw_rect.q.y = fixed_ceiling(draw_rect.q.y);
                }
                x = fixed2int(draw_rect.p.x);
                y = fixed2int(draw_rect.p.y);
                w = fixed2int(draw_rect.q.x) - x;
                h = fixed2int(draw_rect.q.y) - y;
                /* clients that use the "any part of pixel" rule also
                   fill 0 areas.  This is true of current graphics
                   library clients but not a general rule.  */
                if (!center_of_pixel) {
                    if (w == 0)
                        w = 1;
                    /* yes Adobe Acrobat 8, seems to back up the y
                       coordinate when the width is 0, sigh. */
                    if (h == 0) {
                        y--;
                        h = 1;
                    }
                }
                if (gx_fill_rectangle(x, y, w, h, pdc, pgs) < 0)
                    goto slow;
            }
        }
        code = 0;
        goto exit;
      slow:rlist = pr + i;
        rcount = count - i;
    } {
        bool do_save = !gx_path_is_null(pgs->path);

        if (do_save) {
            if ((code = gs_gsave(pgs)) < 0)
                goto exit;
            code = gs_newpath(pgs);
        }
        if ((code >= 0) &&
            (((code = gs_rectappend(pgs, rlist, rcount)) < 0) ||
            ((code = gs_fill(pgs)) < 0))
            )
            DO_NOTHING;
        if (do_save)
            gs_grestore(pgs);
        else if (code < 0)
            gs_newpath(pgs);
    }

exit:
    if (black_vector) {
        /* Restore color */
        gsicc_restore_blacktextvec(pgs, false);
    }
    return code;
}

/* Stroke a list of rectangles. */
/* (We could do this a lot more efficiently.) */
int
gs_rectstroke(gs_gstate * pgs, const gs_rect * pr, uint count,
              const gs_matrix * pmat)
{
    bool do_save = pmat != NULL || !gx_path_is_null(pgs->path);
    int code;

    if (do_save) {
        if ((code = gs_gsave(pgs)) < 0)
            return code;
        gs_newpath(pgs);
    }
    if ((code = gs_rectappend(pgs, pr, count)) < 0 ||
        (pmat != NULL && (code = gs_concat(pgs, pmat)) < 0) ||
        (code = gs_stroke(pgs)) < 0
        )
        DO_NOTHING;
    if (do_save)
        gs_grestore(pgs);
    else if (code < 0)
        gs_newpath(pgs);
    return code;
}
