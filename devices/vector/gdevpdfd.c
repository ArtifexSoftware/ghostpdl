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


/* Path drawing procedures for pdfwrite driver */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxgstate.h"
#include "gxpaint.h"
#include "gxcoord.h"
#include "gxdevmem.h"
#include "gxcolor2.h"
#include "gxhldevc.h"
#include "gsstate.h"
#include "gxstate.h"
#include "gserrors.h"
#include "gsptype2.h"
#include "gsshade.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"
#include "gsutil.h"
#include "gdevpdtf.h"
#include "gdevpdts.h"
#include "gxdevsop.h"

/* ---------------- Drawing ---------------- */

/* Fill a rectangle. */
int
gdev_pdf_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                        gx_color_index color)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code;

    if (pdev->Eps2Write) {
        float x0, y0, x1, y1;
        gs_rect *Box;

        if (!pdev->accumulating_charproc) {
            Box = &pdev->BBox;
            x0 = x / (pdev->HWResolution[0] / 72.0);
            y0 = y / (pdev->HWResolution[1] / 72.0);
            x1 = x0 + (w / (pdev->HWResolution[0] / 72.0));
            y1 = y0 + (h / (pdev->HWResolution[1] / 72.0));
        }
        else {
            Box = &pdev->charproc_BBox;
            x0 = (float)x / 100;
            y0 = (float)y / 100;
            x1 = x0 + (w / 100);
            y1 = y0 + (h / 100);
        }

        if (Box->p.x > x0)
            Box->p.x = x0;
        if (Box->p.y > y0)
            Box->p.y = y0;
        if (Box->q.x < x1)
            Box->q.x = x1;
        if (Box->q.y < y1)
            Box->q.y = y1;
        if (pdev->AccumulatingBBox)
            return 0;
    }
    code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    /* Make sure we aren't being clipped. */
    code = pdf_put_clip_path(pdev, NULL);
    if (code < 0)
        return code;
    pdf_set_pure_color(pdev, color, &pdev->saved_fill_color,
                       &pdev->fill_used_process_color,
                       &psdf_set_fill_color_commands);
    if (!pdev->HaveStrokeColor)
        pdev->saved_stroke_color = pdev->saved_fill_color;
    pprintd4(pdev->strm, "%d %d %d %d re f\n", x, y, w, h);
    return 0;
}

/* ---------------- Path drawing ---------------- */

/* ------ Vector device implementation ------ */

static int
pdf_setlinewidth(gx_device_vector * vdev, double width)
{
    /* Acrobat Reader doesn't accept negative line widths. */
    return psdf_setlinewidth(vdev, fabs(width));
}

static int
pdf_can_handle_hl_color(gx_device_vector * vdev, const gs_gstate * pgs,
                 const gx_drawing_color * pdc)
{
    return pgs != NULL;
}

static int
pdf_setfillcolor(gx_device_vector * vdev, const gs_gstate * pgs,
                 const gx_drawing_color * pdc)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)vdev;
    bool hl_color = (*vdev_proc(vdev, can_handle_hl_color)) (vdev, pgs, pdc);
    const gs_gstate *pgs_for_hl_color = (hl_color ? pgs : NULL);

    if (!pdev->HaveStrokeColor) {
        /* opdfread.ps assumes same color for stroking and non-stroking operations. */
        int code = pdf_set_drawing_color(pdev, pgs_for_hl_color, pdc, &pdev->saved_stroke_color,
                                    &pdev->stroke_used_process_color,
                                    &psdf_set_stroke_color_commands);
        if (code < 0)
            return code;
    }
    return pdf_set_drawing_color(pdev, pgs_for_hl_color, pdc, &pdev->saved_fill_color,
                                 &pdev->fill_used_process_color,
                                 &psdf_set_fill_color_commands);
}

static int
pdf_setstrokecolor(gx_device_vector * vdev, const gs_gstate * pgs,
                   const gx_drawing_color * pdc)
{
    gx_device_pdf *const pdev = (gx_device_pdf *)vdev;
    bool hl_color = (*vdev_proc(vdev, can_handle_hl_color)) (vdev, pgs, pdc);
    const gs_gstate *pgs_for_hl_color = (hl_color ? pgs : NULL);

    if (!pdev->HaveStrokeColor) {
        /* opdfread.ps assumes same color for stroking and non-stroking operations. */
        int code = pdf_set_drawing_color(pdev, pgs_for_hl_color, pdc, &pdev->saved_fill_color,
                                 &pdev->fill_used_process_color,
                                 &psdf_set_fill_color_commands);
        if (code < 0)
            return code;
    }
    return pdf_set_drawing_color(pdev, pgs_for_hl_color, pdc, &pdev->saved_stroke_color,
                                 &pdev->stroke_used_process_color,
                                 &psdf_set_stroke_color_commands);
}

static int
pdf_dorect(gx_device_vector * vdev, fixed x0, fixed y0, fixed x1, fixed y1,
           gx_path_type_t type)
{
    gx_device_pdf *pdev = (gx_device_pdf *)vdev;
    fixed xmax = int2fixed(32766), ymax = int2fixed(32766);
    int bottom = (pdev->ResourcesBeforeUsage ? 1 : 0);
    fixed xmin = (pdev->sbstack_depth > bottom ? -xmax : 0);
    fixed ymin = (pdev->sbstack_depth > bottom ? -ymax : 0);

    /*
     * If we're doing a stroke operation, expand the checking box by the
     * stroke width.
     */
    if (type & gx_path_type_stroke) {
        double w = vdev->state.line_params.half_width;
        double xw = w * (fabs(vdev->state.ctm.xx) + fabs(vdev->state.ctm.yx));
        int d = float2fixed(xw) + fixed_1;

        xmin -= d;
        xmax += d;
        ymin -= d;
        ymax += d;
    }
    if (pdev->PDFA == 1) {
        /* Check values, and decide how to proceed based on PDFACompatibilitylevel */
        if (x0 < xmin || y0 < ymin || x1 - x0 > xmax || y1 - y0 >ymax) {
            switch(pdev->PDFACompatibilityPolicy) {
                case 0:
                    emprintf(pdev->memory,
                         "Required co-ordinate outside valid range for PDF/A-1, reverting to normal PDF output.\n");
                    pdev->AbortPDFAX = true;
                    pdev->PDFA = 0;
                    break;
                case 1:
                    emprintf(pdev->memory,
                         "Required co-ordinate outside valid range for PDF/A-1, clamping to valid range, output may be incorrect.\n");
                    /*
                     * Clamp coordinates to avoid tripping over Acrobat Reader's limit
                     * of 32K on user coordinate values, which was adopted by the PDF/A-1 spec.
                     */
                    if (x0 < xmin)
                        x0 = xmin;

                    if (y0 < ymin)
                        y0 = ymin;

                    /* We used to clamp x1 and y1 here, but actually, because this is a rectangle
                     * we don't need to do that, we need to clamp the *difference* between x0,x1
                     * and y0,y1 to keep it inside the Acrobat 4 or 5 limits.
                     */
                    if (x1 - x0 > xmax)
                        x1 = x0 + xmax;
                    if (y1 - y0 > ymax)
                        y1 = y0 + ymax;
                    break;
                default:
                case 2:
                    emprintf(pdev->memory,
                         "Required co-ordinate outside valid range for PDF/A-1, aborting.\n");
                    return_error(gs_error_limitcheck);
                    break;
            }
        }
    }
    return psdf_dorect(vdev, x0, y0, x1, y1, type);
}

static int
pdf_endpath(gx_device_vector * vdev, gx_path_type_t type)
{
    return 0;			/* always handled by caller */
}

const gx_device_vector_procs pdf_vector_procs = {
        /* Page management */
    NULL,
        /* Imager state */
    pdf_setlinewidth,
    psdf_setlinecap,
    psdf_setlinejoin,
    psdf_setmiterlimit,
    psdf_setdash,
    psdf_setflat,
    psdf_setlogop,
        /* Other state */
    pdf_can_handle_hl_color,
    pdf_setfillcolor,
    pdf_setstrokecolor,
        /* Paths */
    psdf_dopath,
    pdf_dorect,
    psdf_beginpath,
    psdf_moveto,
    psdf_lineto,
    psdf_curveto,
    psdf_closepath,
    pdf_endpath
};

/* ------ Utilities ------ */

/* Store a copy of clipping path. */
int
pdf_remember_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath)
{
    int code = 0;
    /* Used for skipping redundant clip paths. SF bug #624168. */
    if (pdev->clip_path != 0) {
        gx_path_free(pdev->clip_path, "pdf clip path");
    }
    if (pcpath == 0) {
        pdev->clip_path = 0;
        return 0;
    }
    pdev->clip_path = gx_path_alloc(pdev->pdf_memory, "pdf clip path");
    if (pdev->clip_path == 0)
        return_error(gs_error_VMerror);

    code = gx_cpath_to_path((gx_clip_path *)pcpath, pdev->clip_path);
    if (code < 0)
        return code;

    /* gx_cpath_to_path above ends up going through gx_path_assign_preserve
     * which specifically states that the segments of the paths (in this case pcpath
     * and pdev->clip_path) must have been allocated with the same allocator.
     * If that's not true (eg pdfi running inside GS) then we need to 'unshare'
     * the path. Otherwise we mauy end up with pcpath being freed and discarded
     * while the pdfwrite devcie still thinks it has a pointer to it.
     */
    if (pcpath->path.memory != pdev->pdf_memory)
        code = gx_path_unshare(pdev->clip_path);

    return code;
}

/* Check if same clipping path. */
static int
pdf_is_same_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath)
{
    /* Used for skipping redundant clip paths. SF bug #624168. */
    gs_cpath_enum cenum;
    gs_path_enum penum;
    gs_fixed_point vs0[3], vs1[3];
    int code, pe_op;

    if ((pdev->clip_path != 0) != (pcpath != 0))
        return 0;
    /* Both clip paths are empty, so the same */
    if (pdev->clip_path == 0)
        return 1;
    code = gx_path_enum_init(&penum, pdev->clip_path);
    if (code < 0)
        return code;
    code = gx_cpath_enum_init(&cenum, (gx_clip_path *)pcpath);
    if (code < 0)
        return code;
    /* This flags a warning in Coverity, uninitialised variable cenum.first_visit */
    /* This is because gx_cpath_enum_init doesn't initialise first_visit, but the */
    /* variable can be used in enum_next. However, this is not truly used this    */
    /* way. The enum_init sets the 'state' to 'scan', and the first thing that happens */
    /* in enum_next when state is 'scan' is to set first_visit. */
    while ((code = gx_cpath_enum_next(&cenum, vs0)) > 0) {
        pe_op = gx_path_enum_next(&penum, vs1);
        if (pe_op < 0)
            return pe_op;
        if (pe_op != code)
            return 0;
        switch (pe_op) {
            case gs_pe_curveto:
                if (vs0[1].x != vs1[1].x || vs0[1].y != vs1[1].y ||
                    vs0[2].x != vs1[2].x || vs0[2].y != vs1[2].y)
                    return 0;
                /* fall through */
            case gs_pe_moveto:
            case gs_pe_lineto:
            case gs_pe_gapto:
                if (vs0[0].x != vs1[0].x || vs0[0].y != vs1[0].y)
                    return 0;
        }
    }
    if (code < 0)
        return code;
    code = gx_path_enum_next(&penum, vs1);
    if (code < 0)
        return code;
    return (code == 0);
}

int
pdf_check_soft_mask(gx_device_pdf * pdev, gs_gstate * pgs)
{
    int code = 0;

    if (pgs && pdev->state.soft_mask_id != pgs->soft_mask_id) {
    /*
     * The contents must be open already, so the following will only exit
     * text or string context.
     */
    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    if (pdev->vgstack_depth > pdev->vgstack_bottom) {
        code = pdf_restore_viewer_state(pdev, pdev->strm);
        if (code < 0)
            return code;
    }
    }
    return code;
}

/* Test whether we will need to put the clipping path. */
bool
pdf_must_put_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath)
{
    if (pcpath == NULL) {
        if (pdev->clip_path_id == pdev->no_clip_path_id)
            return false;
    } else {
        if (pdev->clip_path_id == pcpath->id)
            return false;
        if (gx_cpath_includes_rectangle(pcpath, fixed_0, fixed_0,
                                        int2fixed(pdev->width),
                                        int2fixed(pdev->height)))
            if (pdev->clip_path_id == pdev->no_clip_path_id)
                return false;
        if (pdf_is_same_clip_path(pdev, pcpath) > 0) {
            pdev->clip_path_id = pcpath->id;
            return false;
        }
    }
    return true;
}

static int pdf_write_path(gx_device_pdf * pdev, gs_path_enum *cenum, gdev_vector_dopath_state_t *state, gx_path *path, int is_clip_enum,
                               gx_path_type_t type, const gs_matrix *pmat)
{
    int pe_op;
    gdev_vector_path_seg_record segments[5] = {0};
    int i, seg_index = 0, is_rect = 1, buffering = 0, initial_m = 0, segs = 0, code, matrix_optimisable = 0;
    gx_path_rectangular_type rtype = prt_none;
    gs_fixed_rect rbox;
    gx_device_vector *vdev = (gx_device_vector *)pdev;
    gs_fixed_point line_start = {0,0};
    gs_point p, q;
    bool do_close = (type & (gx_path_type_clip | gx_path_type_stroke | gx_path_type_always_close)) != 0;
    bool stored_moveto = false;

    gdev_vector_dopath_init(state, (gx_device_vector *)pdev,
                            type, pmat);
    if (is_clip_enum)
        code = gx_cpath_enum_init((gs_cpath_enum *)cenum, (gx_clip_path *)path);
    else {
        code = gx_path_enum_init(cenum, path);
        rtype = gx_path_is_rectangular(path, &rbox);
    }
    if (code < 0)
        return code;

    if((pmat == 0 || is_xxyy(pmat) || is_xyyx(pmat)) &&
        (state->scale_mat.xx == 1.0 && state->scale_mat.yy == 1.0 &&
        is_xxyy(&state->scale_mat) && is_fzero2(state->scale_mat.tx, state->scale_mat.ty))) {
         matrix_optimisable = 1;
         buffering = 1;
    }
    /*
     * if the path type is stroke, we only recognize closed
     * rectangles; otherwise, we recognize all rectangles.
     * Note that for stroking with a transformation, we can't use dorect,
     * which requires (untransformed) device coordinates.
     */
    if (rtype != prt_none &&
        (!(type & gx_path_type_stroke) || rtype == prt_closed) &&
          matrix_optimisable == 1)
    {
        gs_point p, q;

        gs_point_transform_inverse((double)rbox.p.x, (double)rbox.p.y,
                                   &state->scale_mat, &p);
        gs_point_transform_inverse((double)rbox.q.x, (double)rbox.q.y,
                                   &state->scale_mat, &q);
        code = vdev_proc(vdev, dorect)(vdev, (fixed)p.x, (fixed)p.y,
                                       (fixed)q.x, (fixed)q.y, type);
        if (code >= 0) {
            if (code == 0)
                return 1;
            return code;
        }
        /* If the dorect proc failed, use a general path. */
    }

    /* The following is an optimisation for space. If we see a closed subpath
     * which is a rectangle, emit it as a 're' instead of writing the individual
     * segments of the path. Note that 're' always applies the width before the
     * height when constructing the path, so we must take care to ensure that
     * we get the path direction correct. We do ths by detecting whether the path
     * moves first in the x or y directoin, if it moves in the y direction first,
     * we simply move one vertex round the rectangle, ie we start at point 2.
     */
    do {
        if (is_clip_enum)
            segments[seg_index].op = pe_op = gx_cpath_enum_next((gs_cpath_enum *)cenum, segments[seg_index].vs);
        else
            segments[seg_index].op = pe_op = gx_path_enum_next(cenum, segments[seg_index].vs);

        if (segs == 0 && pe_op > 0)
            segs = 1;

        switch(pe_op) {
            case gs_pe_moveto:
            case gs_pe_gapto:
                if (!buffering) {
                    stored_moveto = true;
                    line_start = segments[0].vs[0];
                    seg_index = -1;
                } else {
                    for (i=0;i<seg_index;i++) {
                        gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                    }
                    line_start = segments[seg_index].vs[0];
                    segments[0] = segments[seg_index];
                    seg_index = 0;
                    initial_m = 1;
                }
                break;
            case gs_pe_lineto:
                if (!buffering) {
                    if (stored_moveto) {
                        gdev_vector_dopath_segment(state, gs_pe_moveto, &line_start);
                        stored_moveto = false;
                    }
                    for (i=0;i<=seg_index;i++)
                        gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                    seg_index = -1;
                } else {
                    if (!initial_m) {
                        for (i=0;i<=seg_index;i++) {
                            gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                        }
                        buffering = 0;
                        seg_index = -1;
                    }
                    if (type & gx_path_type_optimize && seg_index > 0) {
                        if (segments[seg_index - 1].op == gs_pe_lineto) {
                            if (segments[seg_index].vs[0].x == segments[seg_index - 1].vs[0].x && segments[seg_index].vs[0].x == line_start.x) {
                                if (segments[seg_index - 1].vs[0].y > line_start.y && segments[seg_index].vs[0].y >= segments[seg_index - 1].vs[0].y) {
                                    segments[seg_index - 1].vs[0].y = segments[seg_index].vs[0].y;
                                    seg_index--;
                                } else {
                                    if (segments[seg_index - 1].vs[0].y < line_start.y && segments[seg_index].vs[0].y <= segments[seg_index - 1].vs[0].y) {
                                        segments[seg_index - 1].vs[0].y = segments[seg_index].vs[0].y;
                                        seg_index--;
                                    } else
                                        line_start = segments[seg_index - 1].vs[0];
                                }
                            } else {
                                if (segments[seg_index].vs[0].y == segments[seg_index - 1].vs[0].y && segments[seg_index].vs[0].y == line_start.y) {
                                    if (segments[seg_index - 1].vs[0].x > line_start.x && segments[seg_index].vs[0].x > segments[seg_index - 1].vs[0].x) {
                                        segments[seg_index - 1].vs[0].x = segments[seg_index].vs[0].x;
                                        seg_index--;
                                    } else {
                                        if (segments[seg_index - 1].vs[0].x < line_start.x && segments[seg_index].vs[0].x < segments[seg_index - 1].vs[0].x) {
                                            segments[seg_index - 1].vs[0].x = segments[seg_index].vs[0].x;
                                            seg_index--;
                                        } else
                                            line_start = segments[seg_index - 1].vs[0];
                                    }
                                } else
                                    line_start = segments[seg_index - 1].vs[0];
                            }
                        }
                    }
                }
                break;
            case gs_pe_curveto:
                if (stored_moveto) {
                    gdev_vector_dopath_segment(state, gs_pe_moveto, &line_start);
                    stored_moveto = false;
                }
                for (i=0;i<=seg_index;i++) {
                    gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                }
                line_start = segments[seg_index].vs[2];
                seg_index = -1;
                buffering = 0;
                break;
            case gs_pe_closepath:
                if (!buffering || seg_index < 4) {
                    if (stored_moveto && ((type & gx_path_type_stroke) && !(type & gx_path_type_fill)))
                        gdev_vector_dopath_segment(state, gs_pe_moveto, &line_start);
                    stored_moveto = false;
                    if (!do_close) {
                        i = seg_index;
                        while (i > 0 && segments[i - 1].op == gs_pe_moveto) {
                            segments[i - 1] = segments[i];
                            i--;
                        }
                        seg_index = i;
                        for (i=0;i<seg_index;i++)
                            gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);

                        seg_index = 0;
                        if (is_clip_enum)
                            segments[seg_index].op = pe_op = gx_cpath_enum_next((gs_cpath_enum *)cenum, segments[seg_index].vs);
                        else
                            segments[seg_index].op = pe_op = gx_path_enum_next(cenum, segments[seg_index].vs);

                        if (pe_op > 0) {
                            gdev_vector_dopath_segment(state, gs_pe_closepath, segments[0].vs);
                            if (pe_op == gs_pe_moveto) {
                                if (matrix_optimisable)
                                    buffering = 1;
                                else
                                    buffering = 0;
                                seg_index = 0;
                                initial_m = 1;
                                line_start = segments[0].vs[0];
                            } else {
                                gdev_vector_dopath_segment(state, segments[0].op, segments[0].vs);
                                buffering = 0;
                                seg_index = -1;
                            }
                        }
                    } else {
                        for (i=0;i<=seg_index;i++)
                            gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                        if (matrix_optimisable)
                            buffering = 1;
                        else
                            buffering = 0;
                        seg_index = -1;
                    }
                } else {
                    is_rect = 1;
                    for (i=1;i<seg_index;i++) {
                        if (segments[i - 1].vs[0].x != segments[i].vs[0].x) {
                            if (segments[i - 1].vs[0].y != segments[i].vs[0].y) {
                                is_rect = 0;
                                break;
                            } else {
                                if (segments[i].vs[0].x != segments[i + 1].vs[0].x || segments[i].vs[0].y == segments[i + 1].vs[0].x){
                                    is_rect = 0;
                                    break;
                                }
                            }
                        } else {
                            if (segments[i - 1].vs[0].y == segments[i].vs[0].y) {
                                is_rect = 0;
                                break;
                            } else {
                                if (segments[i].vs[0].y != segments[i + 1].vs[0].y || segments[i].vs[0].x == segments[i + 1].vs[0].x){
                                    is_rect = 0;
                                    break;
                                }
                            }
                        }
                    }
                    if (segments[0].vs[0].x != segments[seg_index].vs[0].x || segments[0].vs[0].y != segments[seg_index].vs[0].y)
                        is_rect = 0;

                    /* If we would have to alter the starting point, and we are dashing a stroke, then don't treat
                     * this as a rectangle. Changing the start vertex will alter the dash pattern.
                     */
                    if (segments[0].vs[0].x == segments[1].vs[0].x && (type & gx_path_type_dashed_stroke))
                        is_rect = 0;

                    if (is_rect == 1) {
                        gs_fixed_point *pt = &segments[0].vs[0];
                        fixed width, height;

                        if (segments[0].vs[0].x == segments[1].vs[0].x) {
                            pt = &segments[1].vs[0];
                            width = segments[2].vs[0].x - segments[1].vs[0].x;
                            height = segments[0].vs[0].y - segments[1].vs[0].y;
                        } else {
                            width = segments[1].vs[0].x - segments[0].vs[0].x;
                            height = segments[2].vs[0].y - segments[1].vs[0].y;
                        }

                        gs_point_transform_inverse((double)pt->x, (double)pt->y,
                                   &state->scale_mat, &p);
                        gs_point_transform_inverse((double)width, (double)height,
                                   &state->scale_mat, &q);
                        code = vdev_proc(vdev, dorect)(vdev, (fixed)p.x, (fixed)p.y,
                                       (fixed)p.x + (fixed)q.x, (fixed)p.y + (fixed)q.y, type);
                        if (code < 0)
                            return code;
                        seg_index = -1;
                    } else {
                        for (i=0;i<=seg_index;i++) {
                            gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                        }
                        buffering = 0;
                        seg_index = -1;
                    }
                }
                break;
            default:
                for (i=0;i<seg_index;i++)
                    gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
                if (stored_moveto && ((type & gx_path_type_stroke) && !(type & gx_path_type_fill)))
                    gdev_vector_dopath_segment(state, gs_pe_moveto, &line_start);
                seg_index = -1;
                buffering = 0;
                break;
        }
        seg_index++;
        if (seg_index > 4) {
            for (i=0;i<seg_index;i++) {
                gdev_vector_dopath_segment(state, segments[i].op, segments[i].vs);
            }
            seg_index = 0;
            buffering = 0;
        }
    } while (pe_op > 0);

    if (pe_op < 0)
        return pe_op;

    code = vdev_proc(vdev, endpath)(vdev, type);
    return (code < 0 ? code : segs);
}

/* Put a single element of a clipping path list. */
static int
pdf_put_clip_path_list_elem(gx_device_pdf * pdev, gx_cpath_path_list *e,
        gs_path_enum *cenum, gdev_vector_dopath_state_t *state,
        gs_fixed_point vs[3])
{
    int segments = 0;

    /* This function was previously recursive and reversed the order of subpaths. This
     * could lead to a C exec stack overflow on sufficiently complex clipping paths
     * (such as those produced by pdfwrite as a fallback for certain kinds of images).
     * Writing the subpaths in the forward order avoids the problem, is probably
     * slightly faster and uses less memory. Bug #706523.
     */
    while (e) {
        segments = pdf_write_path(pdev, cenum, state, &e->path, 0, gx_path_type_clip | gx_path_type_optimize, NULL);
        if (segments < 0)
            return segments;
        if (segments)
            pprints1(pdev->strm, "%s n\n", (e->rule <= 0 ? "W" : "W*"));
        e = e->next;
    }
    return 0;
}

/* Put a clipping path on the output file. */
int
pdf_put_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath)
{
    int code;
    stream *s = pdev->strm;
    gs_id new_id;

    /* Check for no update needed. */
    if (pcpath == NULL) {
        if (pdev->clip_path_id == pdev->no_clip_path_id)
            return 0;
        new_id = pdev->no_clip_path_id;
    } else {
        if (pdev->clip_path_id == pcpath->id)
            return 0;
        new_id = pcpath->id;
        if (gx_cpath_includes_rectangle(pcpath, fixed_0, fixed_0,
                                        int2fixed(pdev->width),
                                        int2fixed(pdev->height))
            ) {
            if (pdev->clip_path_id == pdev->no_clip_path_id)
                return 0;
            new_id = pdev->no_clip_path_id;
        }
        code = pdf_is_same_clip_path(pdev, pcpath);
        if (code < 0)
            return code;
        if (code) {
            pdev->clip_path_id = new_id;
            return 0;
        }
    }
    /*
     * The contents must be open already, so the following will only exit
     * text or string context.
     */
    code = pdf_open_contents(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    /* Use Q to unwind the old clipping path. */
    if (pdev->vgstack_depth > pdev->vgstack_bottom) {
        code = pdf_restore_viewer_state(pdev, s);
        if (code < 0)
            return code;
    }
    if (new_id != pdev->no_clip_path_id) {
        gs_fixed_rect rect;

        /* Use q to allow the new clipping path to unwind.  */
        code = pdf_save_viewer_state(pdev, s);
        if (code < 0)
            return code;
        /* path_valid states that the clip path is a simple path. If the clip is an intersection of
         * two paths, then path_valid is false. The problem is that the rectangle list is the
         * scan-converted result of the clip, and ths is at the device resolution. Its possible
         * that the intersection of the clips, at device resolution, is rectangular but the
         * two paths are not, and that at a different resolution, nor is the intersection.
         * So we *only* want to write a rectangle, if the clip is rectangular, and its the
         * result of a simple rectangle. Otherwise we want to write the paths that create
         * the clip. However, see below about the path_list.
         */
        if (pcpath->path_valid && cpath_is_rectangle(pcpath, &rect)) {
            /* Use unrounded coordinates. */
            pprintg4(s, "%g %g %g %g re",
                fixed2float(rect.p.x), fixed2float(rect.p.y),
                fixed2float(rect.q.x - rect.p.x),
                fixed2float(rect.q.y - rect.p.y));
            pprints1(s, " %s n\n", (pcpath->rule <= 0 ? "W" : "W*"));
        } else {
            gdev_vector_dopath_state_t state;
            gs_fixed_point vs[3];

            /* the first comment below is (now) incorrect. Previously in gx_clip_to_rectangle()
             * we would create a rectangular clip, without using a path to do so. This results
             * in a rectangular clip, where path_valid is false. However, we did *not* clear
             * the path_list! So if there had previously been a clip path set, by setting paths,
             * we did not clear it. This is not sensible, and caused massive confusion for this code
             * so it has been altered to clear path_list, indicating that there is a clip,
             * the path is not valid, and that it was not created using arbitrary paths.
             * In this case we just emit the rectangle as well (there should be only one).
             */
            if (pcpath->path_list == NULL) {
                /*
                 * We think this should be never executed.
                 * This obsolete branch writes a clip path intersection
                 * as a set of rectangles computed by
                 * gx_cpath_intersect_path_slow.
                 * Those rectangles use coordinates rounded to pixels,
                 * therefore the precision may be unsatisfactory -
                 * see Bug 688407.
                 */
                gs_cpath_enum cenum;

                /*
                 * We have to break 'const' here because the clip path
                 * enumeration logic uses some internal mark bits.
                 * This is very unfortunate, but until we can come up with
                 * a better algorithm, it's necessary.
                 */
                code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)pcpath, 1, gx_path_type_clip | gx_path_type_optimize, NULL);
                if (code < 0)
                    return code;
                pprints1(s, "%s n\n", (pcpath->rule <= 0 ? "W" : "W*"));
            } else {
                gs_path_enum cenum;

                code = pdf_put_clip_path_list_elem(pdev, pcpath->path_list, &cenum, &state, vs);
                if (code < 0)
                    return code;
            }
        }
    }
    pdev->clip_path_id = new_id;
    return pdf_remember_clip_path(pdev,
            (pdev->clip_path_id == pdev->no_clip_path_id ? NULL : pcpath));
}

/*
 * Compute the scaling to ensure that user coordinates for a path are within
 * PDF/A-1 valid range.  Return true if scaling was needed.  In this case, the
 * CTM will be multiplied by *pscale, and all coordinates will be divided by
 * *pscale.
 */
static bool
make_rect_scaling(const gx_device_pdf *pdev, const gs_fixed_rect *bbox,
                  double prescale, double *pscale)
{
    double bmin, bmax;

    if (pdev->PDFA != 1) {
        *pscale = 1;
        return false;
    }

    bmin = min(bbox->p.x / pdev->scale.x, bbox->p.y / pdev->scale.y) * prescale;
    bmax = max(bbox->q.x / pdev->scale.x, bbox->q.y / pdev->scale.y) * prescale;
    if (bmin <= int2fixed(-MAX_USER_COORD) ||
        bmax > int2fixed(MAX_USER_COORD)
        ) {
        /* Rescale the path. */
        *pscale = max(bmin / int2fixed(-MAX_USER_COORD),
                      bmax / int2fixed(MAX_USER_COORD));
        return true;
    } else {
        *pscale = 1;
        return false;
    }
}

/*
 * Prepare a fill with a color anc a clipping path.
 * Return 1 if there is nothing to paint.
 * Changes *box to the clipping box.
 */
static int
prepare_fill_with_clip(gx_device_pdf *pdev, const gs_gstate * pgs,
              gs_fixed_rect *box, bool have_path,
              const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    bool new_clip;
    int code;

    /*
     * Check for an empty clipping path.
     */
    if (pcpath) {
        gs_fixed_rect cbox;

        gx_cpath_outer_box(pcpath, &cbox);
        if (cbox.p.x >= cbox.q.x || cbox.p.y >= cbox.q.y)
            return 1;		/* empty clipping path */
        *box = cbox;
    }
    code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
    if (code < 0)
        return code;

    new_clip = pdf_must_put_clip_path(pdev, pcpath);
    if (have_path || pdev->context == PDF_IN_NONE || new_clip) {
        if (new_clip)
            code = pdf_unclip(pdev);
        else
            code = pdf_open_page(pdev, PDF_IN_STREAM);
        if (code < 0)
            return code;
    }
    code = pdf_prepare_fill(pdev, pgs, false);
    if (code < 0)
        return code;
    return pdf_put_clip_path(pdev, pcpath);
}

/* -------------A local image converter device. -----------------------------*/

public_st_pdf_lcvd_t();

static int
lcvd_copy_color_shifted(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
                      int x, int y, int w, int h)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    int code;
    int dw = cvd->mdev.width;
    int dh = cvd->mdev.height;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    code = cvd->std_copy_color((gx_device *)&cvd->mdev, base, sourcex, sraster, id,
        x - cvd->mdev.mapped_x, y - cvd->mdev.mapped_y, w, h);

    cvd->mdev.width = dw;
    cvd->mdev.height = dh;

    return code;
}

static int
lcvd_copy_mono_shifted(gx_device * dev,
               const byte * base, int sourcex, int sraster, gx_bitmap_id id,
                      int x, int y, int w, int h, gx_color_index zero, gx_color_index one)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    int code;
    int dw = cvd->mdev.width;
    int dh = cvd->mdev.height;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    code = cvd->std_copy_mono((gx_device *)&cvd->mdev, base, sourcex, sraster, id,
                              x - cvd->mdev.mapped_x, y - cvd->mdev.mapped_y, w, h,
                              zero, one);

    cvd->mdev.width = dw;
    cvd->mdev.height = dh;

    return code;
}

static int
lcvd_fill_rectangle_shifted(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    int code;
    int w = cvd->mdev.width;
    int h = cvd->mdev.height;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    code = cvd->std_fill_rectangle((gx_device *)&cvd->mdev,
        x - cvd->mdev.mapped_x, y - cvd->mdev.mapped_y, width, height, color);

    cvd->mdev.width = w;
    cvd->mdev.height = h;

    return code;
}
static int
lcvd_fill_rectangle_shifted2(gx_device *dev, int x, int y, int width, int height, gx_color_index color)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    int code;
    int w = cvd->mdev.width;
    int h = cvd->mdev.height;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    if (cvd->mask) {
        code = (*dev_proc(cvd->mask, fill_rectangle))((gx_device *)cvd->mask,
            x - cvd->mdev.mapped_x, y - cvd->mdev.mapped_y, width, height, (gx_color_index)1);
        if (code < 0)
            goto fail;
    }
    code = cvd->std_fill_rectangle((gx_device *)&cvd->mdev,
        x - cvd->mdev.mapped_x, y - cvd->mdev.mapped_y, width, height, color);

fail:
    cvd->mdev.width = w;
    cvd->mdev.height = h;

    return code;
}
static void
lcvd_get_clipping_box_shifted_from_mdev(gx_device *dev, gs_fixed_rect *pbox)
{
    fixed ofs;
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    int w = cvd->mdev.width;
    int h = cvd->mdev.height;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    cvd->std_get_clipping_box((gx_device *)&cvd->mdev, pbox);
    cvd->mdev.width = w;
    cvd->mdev.height = h;
    ofs = int2fixed(cvd->mdev.mapped_x);
    pbox->p.x += ofs;
    pbox->q.x += ofs;
    ofs = int2fixed(cvd->mdev.mapped_y);
    pbox->p.y += ofs;
    pbox->q.y += ofs;
}
static int
lcvd_dev_spec_op(gx_device *pdev1, int dev_spec_op,
                void *data, int size)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)pdev1;
    int code, w, h;

    switch (dev_spec_op) {
        case gxdso_pattern_shading_area:
            return 1; /* Request shading area. */
        case gxdso_pattern_can_accum:
        case gxdso_pattern_start_accum:
        case gxdso_pattern_finish_accum:
        case gxdso_pattern_load:
        case gxdso_pattern_is_cpath_accum:
        case gxdso_pattern_shfill_doesnt_need_path:
        case gxdso_pattern_handles_clip_path:
        case gxdso_copy_color_is_fast:
            return 0;
    }

    w = cvd->mdev.width;
    h = cvd->mdev.height;
    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    code = gx_default_dev_spec_op(pdev1, dev_spec_op, data, size);

    cvd->mdev.width = w;
    cvd->mdev.height = h;

    return code;
}
static int
lcvd_close_device_with_writing(gx_device *pdev)
{
    /* Assuming 'mdev' is being closed before 'mask' - see gx_image3_end_image. */
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)pdev;
    int code, code1;

    code = pdf_dump_converted_image(cvd->pdev, cvd, 0);
    code1 = cvd->std_close_device((gx_device *)&cvd->mdev);
    return code < 0 ? code : code1;
}

static int
write_image(gx_device_pdf *pdev, gx_device_memory *mdev, gs_matrix *m, int for_pattern)
{
    gs_image_t image;
    pdf_image_writer writer;
    const int sourcex = 0;
    int code;

    if (m != NULL)
        pdf_put_matrix(pdev, NULL, m, " cm\n");
    code = pdf_copy_color_data(pdev, mdev->base, sourcex,
                mdev->raster, gx_no_bitmap_id, 0, 0, mdev->width, mdev->height,
                &image, &writer, for_pattern);
    if (code == 1)
        code = 0; /* Empty image. */
    else if (code == 0)
        code = pdf_do_image(pdev, writer.pres, NULL, true);
    return code;
}
static int
write_mask(gx_device_pdf *pdev, gx_device_memory *mdev, gs_matrix *m)
{
    const int sourcex = 0;
    gs_id save_clip_id = pdev->clip_path_id;
    bool save_skip_color = pdev->skip_colors;
    int code;

    if (m != NULL)
        pdf_put_matrix(pdev, NULL, m, " cm\n");
    pdev->clip_path_id = pdev->no_clip_path_id;
    pdev->skip_colors = true;
    code = gdev_pdf_copy_mono((gx_device *)pdev, mdev->base, sourcex,
                mdev->raster, gx_no_bitmap_id, 0, 0, mdev->width, mdev->height,
                gx_no_color_index, (gx_color_index)0);
    pdev->clip_path_id = save_clip_id;
    pdev->skip_colors = save_skip_color;
    return code;
}

static void
max_subimage_width(int width, byte *base, int x0, long count1, int *x1, long *count)
{
    long c = 0, c1 = count1 - 1;
    int x = x0;
    byte p = 1; /* The inverse of the previous bit. */
    byte r;     /* The inverse of the current  bit. */
    byte *q = base + (x / 8), m = 0x80 >> (x % 8);

    for (; x < width; x++) {
        r = !(*q & m);
        if (p != r) {
            if (c >= c1) {
                if (!r)
                    goto ex; /* stop before the upgrade. */
            }
            c++;
        }
        p = r;
        m >>= 1;
        if (!m) {
            m = 0x80;
            q++;
        }
    }
    if (p)
        c++; /* Account the last downgrade. */
ex:
    *count = c;
    *x1 = x;
}

static int
cmpbits(const byte *base, const byte *base2, int w)
{
    int code;

    code = memcmp(base, base2, w>>3);
    if (code)
        return code;
    base += w>>3;
    base2 += w>>3;
    w &= 7;
    if (w == 0)
        return 0;
    return ((*base ^ *base2) & (0xff00>>w));
}

static void
compute_subimage(int width, int height, int raster, byte *base,
                 int x0, int y0, long MaxClipPathSize, int *x1, int *y1)
{
    /* Returns a semiopen range : [x0:x1)*[y0:y1). */
    if (x0 != 0) {
        long count;

        /* A partial single scanline. */
        max_subimage_width(width, base + y0 * raster, x0, MaxClipPathSize / 4, x1, &count);
        *y1 = y0;
    } else {
        int xx, y = y0, yy;
        long count, count1 = MaxClipPathSize / 4;

        for(; y < height && count1 > 0; ) {
            max_subimage_width(width, base + y * raster, 0, count1, &xx, &count);
            if (xx < width) {
                if (y == y0) {
                    /* Partial single scanline. */
                    *y1 = y + 1;
                    *x1 = xx;
                    return;
                } else {
                    /* Full lines before this scanline. */
                    break;
                }
            }
            count1 -= count;
            yy = y + 1;
            for (; yy < height; yy++)
                if (cmpbits(base + raster * y, base + raster * yy, width))
                    break;
            y = yy;

        }
        *y1 = y;
        *x1 = width;
    }
}

static int
image_line_to_clip(gx_device_pdf *pdev, byte *base, int x0, int x1, int y0, int y1, bool started)
{   /* returns the number of segments or error code. */
    int x = x0, xx;
    byte *q = base + (x / 8), m = 0x80 >> (x % 8);
    long c = 0;

    for (;;) {
        /* Look for upgrade : */
        for (; x < x1; x++) {
            if (*q & m)
                break;
            m >>= 1;
            if (!m) {
                m = 0x80;
                q++;
            }
        }
        if (x == x1)
            return c;
        xx = x;
        /* Look for downgrade : */
        for (; x < x1; x++) {
            if (!(*q & m))
                break;
            m >>= 1;
            if (!m) {
                m = 0x80;
                q++;
            }
        }
        /* Found the interval [xx:x). */
        if (!started) {
            stream_puts(pdev->strm, "n\n");
            started = true;
        }
        pprintld2(pdev->strm, "%ld %ld m ", xx, y0);
        pprintld2(pdev->strm, "%ld %ld l ", x, y0);
        pprintld2(pdev->strm, "%ld %ld l ", x, y1);
        pprintld2(pdev->strm, "%ld %ld l h\n", xx, y1);
        c += 4;
    }
    return c;
}

static int
mask_to_clip(gx_device_pdf *pdev, int width, int height,
             int raster, byte *base, int x0, int y0, int x1, int y1)
{
    int y, yy, code = 0;
    bool has_segments = false;

    for (y = y0; y < y1 && code >= 0;) {
        yy = y + 1;
        if (x0 == 0) {
        for (; yy < y1; yy++)
            if (cmpbits(base + raster * y, base + raster * yy, width))
                break;
        }
        code = image_line_to_clip(pdev, base + raster * y, x0, x1, y, yy, has_segments);
        if (code > 0)
            has_segments = true;
        y = yy;
    }
    if (has_segments)
        stream_puts(pdev->strm, "W n\n");
    return code < 0 ? code : has_segments ? 1 : 0;
}

static int
write_subimage(gx_device_pdf *pdev, gx_device_memory *mdev, int x, int y, int x1, int y1, int for_pattern)
{
    gs_image_t image;
    pdf_image_writer writer;
    /* expand in 1 pixel to provide a proper color interpolation */
    int X = max(0, x - 1);
    int Y = max(0, y - 1);
    int X1 = min(mdev->width, x1 + 1);
    int Y1 = min(mdev->height, y1 + 1);
    int code;

    code = pdf_copy_color_data(pdev, mdev->base + mdev->raster * Y, X,
                mdev->raster, gx_no_bitmap_id,
                X, Y, X1 - X, Y1 - Y,
                &image, &writer, for_pattern);
    if (code < 0)
        return code;
    if (!writer.pres)
        return 0; /* inline image. */
    return pdf_do_image(pdev, writer.pres, NULL, true);
}

static int
write_image_with_clip(gx_device_pdf *pdev, pdf_lcvd_t *cvd, int for_pattern)
{
    int x = 0, y = 0;
    int code, code1;

    if (cvd->write_matrix)
        pdf_put_matrix(pdev, NULL, &cvd->m, " cm q\n");
    for(;;) {
        int x1, y1;

        compute_subimage(cvd->mask->width, cvd->mask->height,
                         cvd->mask->raster, cvd->mask->base,
                         x, y, max(pdev->MaxClipPathSize, 100), &x1, &y1);
        code = mask_to_clip(pdev,
                         cvd->mask->width, cvd->mask->height,
                         cvd->mask->raster, cvd->mask->base,
                         x, y, x1, y1);
        if (code < 0)
            return code;
        if (code > 0) {
            code1 = write_subimage(pdev, &cvd->mdev, x, y, x1, y1, for_pattern);
            if (code1 < 0)
                return code1;
        }
        if (x1 >= cvd->mdev.width && y1 >= cvd->mdev.height)
            break;
        if (code > 0)
            stream_puts(pdev->strm, "Q q\n");
        if (x1 == cvd->mask->width) {
            x = 0;
            y = y1;
        } else {
            x = x1;
            y = y1;
        }
    }
    if (cvd->write_matrix)
        stream_puts(pdev->strm, "Q\n");
    return 0;
}

int
pdf_dump_converted_image(gx_device_pdf *pdev, pdf_lcvd_t *cvd, int for_pattern)
{
    int code = 0;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    if (!cvd->path_is_empty || cvd->has_background) {
        if (!cvd->has_background)
            stream_puts(pdev->strm, "W n\n");
        code = write_image(pdev, &cvd->mdev, (cvd->write_matrix ? &cvd->m : NULL), for_pattern);
        cvd->path_is_empty = true;
    } else if (!cvd->mask_is_empty && pdev->PatternImagemask) {
        /* Convert to imagemask with a pattern color. */
        /* See also use_image_as_pattern in gdevpdfi.c . */
        gs_gstate s;
        gs_pattern1_instance_t inst;
        gs_id id = gs_next_ids(cvd->mdev.memory, 1);
        cos_value_t v;
        const pdf_resource_t *pres;

        memset(&s, 0, sizeof(s));
        s.ctm.xx = cvd->m.xx;
        s.ctm.xy = cvd->m.xy;
        s.ctm.yx = cvd->m.yx;
        s.ctm.yy = cvd->m.yy;
        s.ctm.tx = cvd->m.tx;
        s.ctm.ty = cvd->m.ty;
        memset(&inst, 0, sizeof(inst));
        inst.saved = (gs_gstate *)&s; /* HACK : will use s.ctm only. */
        inst.templat.PaintType = 1;
        inst.templat.TilingType = 1;
        inst.templat.BBox.p.x = inst.templat.BBox.p.y = 0;
        inst.templat.BBox.q.x = cvd->mdev.width;
        inst.templat.BBox.q.y = cvd->mdev.height;
        inst.templat.XStep = (float)cvd->mdev.width;
        inst.templat.YStep = (float)cvd->mdev.height;

        {
            pattern_accum_param_s param;
            param.pinst = (void *)&inst;
            param.graphics_state = (void *)&s;
            param.pinst_id = inst.id;

            code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
                gxdso_pattern_start_accum, &param, sizeof(pattern_accum_param_s));
        }

        if (code >= 0) {
            stream_puts(pdev->strm, "W n\n");
            code = write_image(pdev, &cvd->mdev, NULL, for_pattern);
        }
        pres = pdev->accumulating_substream_resource;
        if (code >= 0) {
            pattern_accum_param_s param;
            param.pinst = (void *)&inst;
            param.graphics_state = (void *)&s;
            param.pinst_id = inst.id;

            code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
                gxdso_pattern_finish_accum, &param, id);
        }
        if (code >= 0)
            code = (*dev_proc(pdev, dev_spec_op))((gx_device *)pdev,
                gxdso_pattern_load, &id, sizeof(gs_id));
        if (code >= 0)
            code = pdf_cs_Pattern_colored(pdev, &v);
        if (code >= 0) {
            cos_value_write(&v, pdev);
            pprintld1(pdev->strm, " cs /R%ld scn ", pdf_resource_id(pres));
        }
        if (code >= 0)
            code = write_mask(pdev, cvd->mask, (cvd->write_matrix ? &cvd->m : NULL));
        cvd->mask_is_empty = true;
    } else if (!cvd->mask_is_empty && !pdev->PatternImagemask) {
        /* Convert to image with a clipping path. */
        stream_puts(pdev->strm, "q\n");
        code = write_image_with_clip(pdev, cvd, for_pattern);
        stream_puts(pdev->strm, "Q\n");
    } else if (cvd->filled_trap){
        stream_puts(pdev->strm, "q\n");
        code = write_image_with_clip(pdev, cvd, for_pattern);
        stream_puts(pdev->strm, "Q\n");
    }
    cvd->filled_trap = false;
    cvd->mdev.width += cvd->mdev.mapped_x;
    cvd->mdev.height += cvd->mdev.mapped_y;
    if (code > 0)
        code = (*dev_proc(&cvd->mdev, fill_rectangle))((gx_device *)&cvd->mdev,
                0, 0, cvd->mdev.width, cvd->mdev.height, (gx_color_index)0);
    return code;
}
static int
lcvd_fill_trapezoid(gx_device * dev, const gs_fixed_edge * left,
    const gs_fixed_edge * right, fixed ybot, fixed ytop, bool swap_axes,
    const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;

    if (cvd->mask != NULL)
        cvd->filled_trap = true;
    return gx_default_fill_trapezoid(dev, left, right, ybot, ytop, swap_axes, pdevc, lop);
}

static int
lcvd_handle_fill_path_as_shading_coverage(gx_device *dev,
    const gs_gstate *pgs, gx_path *ppath,
    const gx_fill_params *params,
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath)
{
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    gx_device_pdf *pdev = (gx_device_pdf *)cvd->mdev.target;
    int code;

    if (cvd->has_background)
        return 0;
    if (gx_path_is_null(ppath)) {
        /* use the mask. */
        if (!cvd->path_is_empty) {
            code = pdf_dump_converted_image(pdev, cvd, 2);
            if (code < 0)
                return code;
            stream_puts(pdev->strm, "Q q\n");
            dev_proc(&cvd->mdev, fill_rectangle) = lcvd_fill_rectangle_shifted2;
        }
        if (cvd->mask && (!cvd->mask_is_clean || !cvd->path_is_empty)) {
            code = (*dev_proc(cvd->mask, fill_rectangle))((gx_device *)cvd->mask,
                        0, 0, cvd->mask->width, cvd->mask->height, (gx_color_index)0);
            if (code < 0)
                return code;
            cvd->mask_is_clean = true;
        }
        cvd->path_is_empty = true;
        if (cvd->mask)
            cvd->mask_is_empty = false;
    } else {
        gs_matrix m;
        gs_path_enum cenum;
        gdev_vector_dopath_state_t state;

        gs_make_translation(cvd->path_offset.x, cvd->path_offset.y, &m);
        /* use the clipping. */
        if (!cvd->mask_is_empty) {
            code = pdf_dump_converted_image(pdev, cvd, 2);
            if (code < 0)
                return code;
            stream_puts(pdev->strm, "Q q\n");
            dev_proc(&cvd->mdev, fill_rectangle) = lcvd_fill_rectangle_shifted;
            cvd->mask_is_empty = true;
        }
        code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_fill | gx_path_type_optimize, &m);
        if (code < 0)
            return code;
        stream_puts(pdev->strm, "h\n");
        cvd->path_is_empty = false;
    }
    return 0;
}

static int
lcvd_transform_pixel_region(gx_device *dev, transform_pixel_region_reason reason, transform_pixel_region_data *data)
{
    transform_pixel_region_data local_data;
    gx_dda_fixed_point local_pixels, local_rows;
    gs_int_rect local_clip;
    pdf_lcvd_t *cvd = (pdf_lcvd_t *)dev;
    int ret;
    dev_t_proc_fill_rectangle((*fill_rectangle), gx_device);
    dev_t_proc_copy_color((*copy_color), gx_device);
    int w = cvd->mdev.width;
    int h = cvd->mdev.height;

    cvd->mdev.width -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;

    if (reason == transform_pixel_region_begin) {
        local_data = *data;
        local_pixels = *local_data.u.init.pixels;
        local_rows = *local_data.u.init.rows;
        local_clip = *local_data.u.init.clip;
        local_data.u.init.pixels = &local_pixels;
        local_data.u.init.rows = &local_rows;
        local_data.u.init.clip = &local_clip;
        local_pixels.x.state.Q -= int2fixed(cvd->mdev.mapped_x);
        local_pixels.y.state.Q -= int2fixed(cvd->mdev.mapped_y);
        local_rows.x.state.Q -= int2fixed(cvd->mdev.mapped_x);
        local_rows.y.state.Q -= int2fixed(cvd->mdev.mapped_y);
        local_clip.p.x -= cvd->mdev.mapped_x;
        local_clip.p.y -= cvd->mdev.mapped_y;
        local_clip.q.x -= cvd->mdev.mapped_x;
        local_clip.q.y -= cvd->mdev.mapped_y;
        ret = cvd->std_transform_pixel_region(dev, reason, &local_data);
        data->state = local_data.state;
        cvd->mdev.width = w;
        cvd->mdev.height = h;
        return ret;
    }
    copy_color = dev_proc(&cvd->mdev, copy_color);
    fill_rectangle = dev_proc(&cvd->mdev, fill_rectangle);
    dev_proc(&cvd->mdev, copy_color) = cvd->std_copy_color;
    dev_proc(&cvd->mdev, fill_rectangle) = cvd->std_fill_rectangle;
    ret = cvd->std_transform_pixel_region(dev, reason, data);
    dev_proc(&cvd->mdev, copy_color) = copy_color;
    dev_proc(&cvd->mdev, fill_rectangle) = fill_rectangle;
    cvd->mdev.width = w;
    cvd->mdev.height = h;
    return ret;
}

int
pdf_setup_masked_image_converter(gx_device_pdf *pdev, gs_memory_t *mem, const gs_matrix *m, pdf_lcvd_t **pcvd,
                                 bool need_mask, int x, int y, int w, int h, bool write_on_close)
{
    int code;
    gx_device_memory *mask = 0;
    pdf_lcvd_t *cvd = *pcvd;

    if (cvd == NULL) {
        cvd = gs_alloc_struct(mem, pdf_lcvd_t, &st_pdf_lcvd_t, "pdf_setup_masked_image_converter");
        if (cvd == NULL)
            return_error(gs_error_VMerror);
        *pcvd = cvd;
    }
    cvd->pdev = pdev;
    gs_make_mem_device(&cvd->mdev, gdev_mem_device_for_bits(pdev->color_info.depth),
                mem, 0, (gx_device *)pdev);
    /* x and y are always non-negative here. */
    if (x < 0 || y < 0)
        return_error(gs_error_Fatal);
    cvd->mdev.width  = w; /* The size we want to allocate for */
    cvd->mdev.height = h;
    cvd->mdev.mapped_x = x;
    cvd->mdev.mapped_y = y;
    cvd->mdev.bitmap_memory = mem;
    cvd->mdev.color_info = pdev->color_info;
    cvd->path_is_empty = true;
    cvd->mask_is_empty = true;
    cvd->mask_is_clean = false;
    cvd->filled_trap = false;
    cvd->has_background = false;
    cvd->mask = 0;
    cvd->write_matrix = true;
    code = (*dev_proc(&cvd->mdev, open_device))((gx_device *)&cvd->mdev);
    if (code < 0)
        return code; /* FIXME: free cvd? */
    code = (*dev_proc(&cvd->mdev, fill_rectangle))((gx_device *)&cvd->mdev,
                0, 0, cvd->mdev.width, cvd->mdev.height, (gx_color_index)0);
    if (code < 0)
        return code; /* FIXME: free cvd? */
    if (need_mask) {
        mask = gs_alloc_struct_immovable(mem, gx_device_memory, &st_device_memory, "pdf_setup_masked_image_converter");
        if (mask == NULL)
            return_error(gs_error_VMerror); /* FIXME: free cvd? */
        cvd->mask = mask;
        gs_make_mem_mono_device(mask, mem, (gx_device *)pdev);
        mask->width = cvd->mdev.width;
        mask->height = cvd->mdev.height;
        mask->raster = gx_device_raster((gx_device *)mask, 1);
        mask->bitmap_memory = mem;
        code = (*dev_proc(mask, open_device))((gx_device *)mask);
        if (code < 0)
            return code; /* FIXME: free cvd? */
        if (write_on_close) {
            code = (*dev_proc(mask, fill_rectangle))((gx_device *)mask,
                        0, 0, mask->width, mask->height, (gx_color_index)0);
            if (code < 0)
                return code; /* FIXME: free cvd? */
        }
    }
    cvd->std_copy_color = dev_proc(&cvd->mdev, copy_color);
    cvd->std_copy_mono = dev_proc(&cvd->mdev, copy_mono);
    cvd->std_fill_rectangle = dev_proc(&cvd->mdev, fill_rectangle);
    cvd->std_close_device = dev_proc(&cvd->mdev, close_device);
    cvd->std_get_clipping_box = dev_proc(&cvd->mdev, get_clipping_box);
    cvd->std_transform_pixel_region = dev_proc(&cvd->mdev, transform_pixel_region);
    if (!write_on_close) {
        /* Type 3 images will write to the mask directly. */
        dev_proc(&cvd->mdev, fill_rectangle) = (need_mask ? lcvd_fill_rectangle_shifted2
                                                          : lcvd_fill_rectangle_shifted);
    } else {
        dev_proc(&cvd->mdev, fill_rectangle) = lcvd_fill_rectangle_shifted;
    }
    dev_proc(&cvd->mdev, get_clipping_box) = lcvd_get_clipping_box_shifted_from_mdev;
    dev_proc(&cvd->mdev, copy_color) = lcvd_copy_color_shifted;
    dev_proc(&cvd->mdev, copy_mono) = lcvd_copy_mono_shifted;
    dev_proc(&cvd->mdev, dev_spec_op) = lcvd_dev_spec_op;
    dev_proc(&cvd->mdev, fill_path) = lcvd_handle_fill_path_as_shading_coverage;
    dev_proc(&cvd->mdev, transform_pixel_region) = lcvd_transform_pixel_region;
    dev_proc(&cvd->mdev, fill_trapezoid) = lcvd_fill_trapezoid;
    cvd->m = *m;
    if (write_on_close) {
        cvd->mdev.is_open = true;
        if (mask)
            mask->is_open = true;
        dev_proc(&cvd->mdev, close_device) = lcvd_close_device_with_writing;
    }
    cvd->mdev.width  = w + x; /* The size we appear to the world as */
    cvd->mdev.height = h + y;
    return 0;
}

void
pdf_remove_masked_image_converter(gx_device_pdf *pdev, pdf_lcvd_t *cvd, bool need_mask)
{
    cvd->mdev.width  -= cvd->mdev.mapped_x;
    cvd->mdev.height -= cvd->mdev.mapped_y;
    (*dev_proc(&cvd->mdev, close_device))((gx_device *)&cvd->mdev);
    if (cvd->mask) {
        (*dev_proc(cvd->mask, close_device))((gx_device *)cvd->mask);
        gs_free_object(cvd->mask->memory, cvd->mask, "pdf_remove_masked_image_converter");
    }
}

/* ------ Driver procedures ------ */

/* Fill a path. */
int
gdev_pdf_fill_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
                   const gx_fill_params * params,
              const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code;
    /*
     * HACK: we fill an empty path in order to set the clipping path
     * and the color for writing text.  If it weren't for this, we
     * could detect and skip empty paths before putting out the clip
     * path or the color.  We also clip with an empty path in order
     * to advance currentpoint for show operations without actually
     * drawing anything.
     */
    bool have_path;
    gs_fixed_rect box = {{0, 0}, {0, 0}}, box1;
    gs_rect box2;

    if (pdev->Eps2Write) {
        gx_path_bbox(ppath, &box1);
        if (box1.p.x != 0 || box1.p.y != 0 || box1.q.x != 0 || box1.q.y != 0){
            if (pcpath != 0)
                rect_intersect(box1, pcpath->outer_box);
            /* convert fixed point co-ordinates to floating point and account for resolution */
            box2.p.x = fixed2int(box1.p.x) / (pdev->HWResolution[0] / 72.0);
            box2.p.y = fixed2int(box1.p.y) / (pdev->HWResolution[1] / 72.0);
            box2.q.x = fixed2int(box1.q.x) / (pdev->HWResolution[0] / 72.0);
            box2.q.y = fixed2int(box1.q.y) / (pdev->HWResolution[1] / 72.0);
            /* Finally compare the new BBox of the path with the existing EPS BBox */
            if (box2.p.x < pdev->BBox.p.x)
                pdev->BBox.p.x = box2.p.x;
            if (box2.p.y < pdev->BBox.p.y)
                pdev->BBox.p.y = box2.p.y;
            if (box2.q.x > pdev->BBox.q.x)
                pdev->BBox.q.x = box2.q.x;
            if (box2.q.y > pdev->BBox.q.y)
                pdev->BBox.q.y = box2.q.y;
        }
        if (pdev->AccumulatingBBox)
            return 0;
    }
    have_path = !gx_path_is_void(ppath);
    if (!have_path && !pdev->vg_initial_set) {
        /* See lib/gs_pdfwr.ps about "initial graphic state". */
        pdf_prepare_initial_viewer_state(pdev, pgs);
        pdf_reset_graphics(pdev);
        return 0;
    }
    if (have_path) {
        code = gx_path_bbox(ppath, &box);
        if (code < 0)
            return code;
    }
    box1 = box;

    code = prepare_fill_with_clip(pdev, pgs, &box, have_path, pdcolor, pcpath);
    if (code == gs_error_rangecheck) {
        /* Fallback to the default implermentation for handling
           a transparency with CompatibilityLevel<=1.3 . */
        return gx_default_fill_path((gx_device *)pdev, pgs, ppath, params, pdcolor, pcpath);
    }
    if (code < 0)
        return code;
    if (code == 1)
        return 0; /* Nothing to paint. */
    if (!have_path)
        return 0;
    code = pdf_setfillcolor((gx_device_vector *)pdev, pgs, pdcolor);
    if (code == gs_error_rangecheck) {
        const bool convert_to_image = ((pdev->CompatibilityLevel <= 1.2 ||
                pdev->params.ColorConversionStrategy != ccs_LeaveColorUnchanged) &&
                gx_dc_is_pattern2_color(pdcolor));

        if (!convert_to_image) {
            /* Fallback to the default implermentation for handling
            a shading with CompatibilityLevel<=1.2 . */
            return gx_default_fill_path(dev, pgs, ppath, params, pdcolor, pcpath);
        } else {
            /* Convert a shading into a bitmap
               with CompatibilityLevel<=1.2 . */
            pdf_lcvd_t cvd, *pcvd = &cvd;
            int sx, sy;
            gs_fixed_rect bbox, bbox1;
            bool need_mask = gx_dc_pattern2_can_overlap(pdcolor);
            gs_matrix m, save_ctm = ctm_only(pgs), ms, msi, mm;
            gs_int_point rect_size;
            /* double scalex = 1.9, scaley = 1.4; debug purpose only. */
            double scale, scalex, scaley;
            int log2_scale_x = 0, log2_scale_y = 0;
            gx_drawing_color dc = *pdcolor;
            gs_pattern2_instance_t pi = *(gs_pattern2_instance_t *)dc.ccolor.pattern;
            gs_gstate *pgs2 = gs_gstate_copy(pi.saved, gs_gstate_memory(pi.saved));

            if (pgs2 == NULL)
                return_error(gs_error_VMerror);
            dc.ccolor.pattern = (gs_pattern_instance_t *)&pi;
            pi.saved = pgs2;
            code = gx_path_bbox(ppath, &bbox);
            if (code < 0)
                goto image_exit;
            rect_intersect(bbox, box);
            code = gx_dc_pattern2_get_bbox(pdcolor, &bbox1);
            if (code < 0)
                goto image_exit;
            if (code)
                rect_intersect(bbox, bbox1);
            if (bbox.p.x >= bbox.q.x || bbox.p.y >= bbox.q.y) {
                code = 0;
                goto image_exit;
            }
            sx = fixed2int(bbox.p.x);
            sy = fixed2int(bbox.p.y);
            gs_make_identity(&m);
            rect_size.x = fixed2int(bbox.q.x + fixed_half) - sx;
            rect_size.y = fixed2int(bbox.q.y + fixed_half) - sy;
            if (rect_size.x == 0 || rect_size.y == 0)
                goto image_exit;
            m.tx = (float)sx;
            m.ty = (float)sy;
            cvd.path_offset.x = sx;
            cvd.path_offset.y = sy;
            scale = (double)rect_size.x * rect_size.y * pdev->color_info.num_components /
                    pdev->MaxShadingBitmapSize;
            if (scale > 1) {
                /* This section (together with the call to 'path_scale' below)
                   sets up a downscaling when converting the shading into bitmap.
                   We used floating point numbers to debug it, but in production
                   we prefer to deal only with integers being powers of 2
                   in order to avoid possible distorsions when scaling paths.
                */
                log2_scale_x = log2_scale_y = ilog2((int)ceil(sqrt(scale)));
                if ((double)(1 << log2_scale_x) * (1 << log2_scale_y) < scale)
                    log2_scale_y++;
                if ((double)(1 << log2_scale_x) * (1 << log2_scale_y) < scale)
                    log2_scale_x++;
                scalex = (double)(1 << log2_scale_x);
                scaley = (double)(1 << log2_scale_y);
                rect_size.x = (int)floor(rect_size.x / scalex + 0.5);
                rect_size.y = (int)floor(rect_size.y / scaley + 0.5);
                gs_make_scaling(1.0 / scalex, 1.0 / scaley, &ms);
                gs_make_scaling(scalex, scaley, &msi);
                gs_matrix_multiply(&msi, &m, &m);
                gs_matrix_multiply(&ctm_only(pgs), &ms, &mm);
                gs_setmatrix((gs_gstate *)pgs, &mm);
                gs_matrix_multiply(&ctm_only(pgs2), &ms, &mm);
                gs_setmatrix((gs_gstate *)pgs2, &mm);
                sx = fixed2int(bbox.p.x / (int)scalex);
                sy = fixed2int(bbox.p.y / (int)scaley);
                cvd.path_offset.x = sx; /* m.tx / scalex */
                cvd.path_offset.y = sy;
            }
            code = pdf_setup_masked_image_converter(pdev, pdev->memory, &m, &pcvd, need_mask, sx, sy,
                            rect_size.x, rect_size.y, false);
            pcvd->has_background = gx_dc_pattern2_has_background(pdcolor);
            stream_puts(pdev->strm, "q\n");
            if (code >= 0) {
                gs_path_enum cenum;
                gdev_vector_dopath_state_t state;
                code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_clip | gx_path_type_optimize, NULL);
                if (code >= 0)
                    stream_puts(pdev->strm, (params->rule < 0 ? "W n\n" : "W* n\n"));
            }
            pdf_put_matrix(pdev, NULL, &cvd.m, " cm q\n");
            cvd.write_matrix = false;
            if (code >= 0)
                code = gs_shading_do_fill_rectangle(pi.templat.Shading,
                     NULL, (gx_device *)&cvd.mdev, pgs2, !pi.shfill);
            if (code >= 0)
                code = pdf_dump_converted_image(pdev, &cvd, 2);
            stream_puts(pdev->strm, "Q Q\n");
            pdf_remove_masked_image_converter(pdev, &cvd, need_mask);
            gs_setmatrix((gs_gstate *)pgs, &save_ctm);
image_exit:
            gs_gstate_free(pgs2);
            return code;
        }
    }
    if (code < 0)
        return code;
    {
        stream *s = pdev->strm;
        double scale;
        gs_matrix smat, *psmat = NULL;
        gs_path_enum cenum;
        gdev_vector_dopath_state_t state;

        if (pcpath) {
            rect_intersect(box1, box);
            if (box1.p.x > box1.q.x || box1.p.y > box1.q.y)
                return 0;		/* outside the clipping path */
        }
        if (params->flatness != pdev->state.flatness) {
            pprintg1(s, "%g i\n", params->flatness);
            pdev->state.flatness = params->flatness;
        }
        if (make_rect_scaling(pdev, &box1, 1.0, &scale)) {
            gs_make_scaling(pdev->scale.x * scale, pdev->scale.y * scale,
                            &smat);
            pdf_put_matrix(pdev, "q ", &smat, "cm\n");
            psmat = &smat;
        }
        code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_fill | gx_path_type_optimize, NULL);
        if (code < 0)
            return code;

        stream_puts(s, (params->rule < 0 ? "f\n" : "f*\n"));
        if (psmat != NULL)
            stream_puts(pdev->strm, "Q\n");
    }
    return 0;
}

/* Stroke a path. */
int
gdev_pdf_stroke_path(gx_device * dev, const gs_gstate * pgs,
                     gx_path * ppath, const gx_stroke_params * params,
              const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    stream *s;
    int code;
    double scale, path_scale;
    bool set_ctm;
    gs_matrix mat;
    double prescale = 1;
    gs_fixed_rect bbox;
    gs_path_enum cenum;
    gdev_vector_dopath_state_t state;

    if (gx_path_is_void(ppath))
        return 0;		/* won't mark the page */
    code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
    if (code < 0)
        return code;
   if (pdf_must_put_clip_path(pdev, pcpath))
        code = pdf_unclip(pdev);
    else if ((pdev->last_charpath_op & TEXT_DO_FALSE_CHARPATH) && ppath->current_subpath &&
        (ppath->last_charpath_segment == ppath->current_subpath->last) && !pdev->ForOPDFRead) {
        bool hl_color = pdf_can_handle_hl_color((gx_device_vector *)pdev, pgs, pdcolor);
        const gs_gstate *pgs_for_hl_color = (hl_color ? pgs : NULL);

        if (pdf_modify_text_render_mode(pdev->text->text_state, 1)) {
            /* Set the colour for the stroke */
            code = pdf_reset_color(pdev, pgs_for_hl_color, pdcolor, &pdev->saved_stroke_color,
                        &pdev->stroke_used_process_color, &psdf_set_stroke_color_commands);
            if (code == 0) {
                s = pdev->strm;
                /* Text is emitted scaled so that the CTM is an identity matrix, the line width
                 * needs to be scaled to match otherwise we will get the default, or the current
                 * width scaled by the CTM before the text, either of which would be wrong.
                 */
                scale = 72 / pdev->HWResolution[0];
                scale *= fabs(pgs->ctm.xx);
                pprintg1(s, "%g w\n", (pgs->line_params.half_width * 2) * (float)scale);
                /* Some trickery here. We have altered the colour, text render mode and linewidth,
                 * we don't want those to persist. By switching to a stream context we will flush the
                 * pending text. This has the beneficial side effect of executing a grestore. So
                 * everything works out neatly.
                 */
                code = pdf_open_page(pdev, PDF_IN_STREAM);
                return(code);
            }
        }
        /* Can only get here if any of the above steps fail, in which case we proceed to
         * emit the charpath as a normal path, and stroke it.
         */
        code = pdf_open_page(pdev, PDF_IN_STREAM);
    } else
        code = pdf_open_page(pdev, PDF_IN_STREAM);
    if (code < 0)
        return code;
    code = pdf_prepare_stroke(pdev, pgs, false);
    if (code == gs_error_rangecheck) {
        /* Fallback to the default implermentation for handling
           a transparency with CompatibilityLevel<=1.3 . */
        return gx_default_stroke_path((gx_device *)dev, pgs, ppath, params, pdcolor, pcpath);
    }
    if (code < 0)
        return code;
    code = pdf_put_clip_path(pdev, pcpath);
    if (code < 0)
        return code;
    /*
     * If the CTM is not uniform, stroke width depends on angle.
     * We'd like to avoid resetting the CTM, so we check for uniform
     * CTMs explicitly.  Note that in PDF, unlike PostScript, it is
     * the CTM at the time of the stroke operation, not the CTM at
     * the time the path was constructed, that is used for transforming
     * the points of the path; so if we have to reset the CTM, we must
     * do it before constructing the path, and inverse-transform all
     * the coordinates.
     */
    set_ctm = (bool)gdev_vector_stroke_scaling((gx_device_vector *)pdev,
                                               pgs, &scale, &mat);
    if (set_ctm && ((pgs->ctm.xx == 0 && pgs->ctm.xy == 0) ||
                    (pgs->ctm.yx == 0 && pgs->ctm.yy == 0))) {
        /* Acrobat Reader 5 and Adobe Reader 6 issues
           the "Wrong operand type" error with matrices, which have 3 zero coefs.
           Besides that, we found that Acrobat Reader 4, Acrobat Reader 5
           and Adobe Reader 6 all store the current path in user space
           and apply CTM in the time of stroking - See the bug 687901.
           Therefore a precise conversion of Postscript to PDF isn't possible in this case.
           Adobe viewers render a line with a constant width instead.
           At last, with set_ctm == true we need the inverse matrix in
           gdev_vector_dopath. Therefore we exclude projection matrices
           (see bug 688363). */
        set_ctm = false;
        scale = fabs(pgs->ctm.xx + pgs->ctm.xy + pgs->ctm.yx + pgs->ctm.yy) /* Using the non-zero coeff. */
                / sqrt(2); /* Empirically from Adobe. */
    }
    if (set_ctm && pdev->PDFA == 1) {
        /*
         * We want a scaling factor that will bring the largest reasonable
         * user coordinate within bounds.  We choose a factor based on the
         * minor axis of the transformation.  Thanks to Raph Levien for
         * the following formula.
         */
        double a = mat.xx, b = mat.xy, c = mat.yx, d = mat.yy;
        double u = fabs(a * d - b * c);
        double v = a * a + b * b + c * c + d * d;
        double minor = (sqrt(v + 2 * u) - sqrt(v - 2 * u)) * 0.5;

        prescale = (minor == 0 || minor > 1 ? 1 : 1 / minor);
    }
    gx_path_bbox(ppath, &bbox);
    {
        /* Check whether a painting appears inside the clipping box.
           Doing so after writing the clipping path due to /SP pdfmark
           uses a special hack with painting outside the clipping box
           for synchronizing the clipping path (see lib/gs_pdfwr.ps).
           That hack appeared because there is no way to pass
           the gs_gstate through gdev_pdf_put_params,
           which pdfmark is implemented with.
        */
        gs_fixed_rect clip_box, stroke_bbox = bbox;
        gs_point d0, d1;
        gs_fixed_point p0, p1;
        fixed bbox_expansion_x, bbox_expansion_y;

        gs_distance_transform(pgs->line_params.half_width, 0, &ctm_only(pgs), &d0);
        gs_distance_transform(0, pgs->line_params.half_width, &ctm_only(pgs), &d1);
        p0.x = float2fixed(any_abs(d0.x));
        p0.y = float2fixed(any_abs(d0.y));
        p1.x = float2fixed(any_abs(d1.x));
        p1.y = float2fixed(any_abs(d1.y));
        bbox_expansion_x = max(p0.x, p1.x) + fixed_1 * 2;
        bbox_expansion_y = max(p0.y, p1.y) + fixed_1 * 2;
        stroke_bbox.p.x -= bbox_expansion_x;
        stroke_bbox.p.y -= bbox_expansion_y;
        stroke_bbox.q.x += bbox_expansion_x;
        stroke_bbox.q.y += bbox_expansion_y;
        gx_cpath_outer_box(pcpath, &clip_box);
        rect_intersect(stroke_bbox, clip_box);
        if (stroke_bbox.q.x < stroke_bbox.p.x || stroke_bbox.q.y < stroke_bbox.p.y)
            return 0;
    }
    if (make_rect_scaling(pdev, &bbox, prescale, &path_scale)) {
        scale /= path_scale;
        if (set_ctm)
            gs_matrix_scale(&mat, path_scale, path_scale, &mat);
        else {
            gs_make_scaling(path_scale, path_scale, &mat);
            set_ctm = true;
        }
    }
    code = gdev_vector_prepare_stroke((gx_device_vector *)pdev, pgs, params,
                                      pdcolor, scale);
    if (code < 0)
        return gx_default_stroke_path(dev, pgs, ppath, params, pdcolor,
                                      pcpath);
    if (!pdev->HaveStrokeColor)
        pdev->saved_fill_color = pdev->saved_stroke_color;
    if (set_ctm)
        pdf_put_matrix(pdev, "q ", &mat, "cm\n");
    if (pgs->line_params.dash.offset != 0 || pgs->line_params.dash.pattern_size != 0)
        code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_stroke | gx_path_type_optimize | gx_path_type_dashed_stroke, (set_ctm ? &mat : (const gs_matrix *)0));
    else
        code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_stroke | gx_path_type_optimize, (set_ctm ? &mat : (const gs_matrix *)0));
    if (code < 0)
        return code;
    s = pdev->strm;
    stream_puts(s, "S");
    stream_puts(s, (set_ctm ? " Q\n" : "\n"));
    if (pdev->Eps2Write) {
        pdev->AccumulatingBBox++;
        code = gx_default_stroke_path(dev, pgs, ppath, params, pdcolor,
                                      pcpath);
        pdev->AccumulatingBBox--;
        if (code < 0)
            return code;
    }
    return 0;
}

int
gdev_pdf_fill_stroke_path(gx_device *dev, const gs_gstate *pgs, gx_path *ppath,
                          const gx_fill_params *fill_params, const gx_drawing_color *pdcolor_fill,
                          const gx_stroke_params *stroke_params, const gx_drawing_color *pdcolor_stroke,
    const gx_clip_path *pcpath)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code;
    bool new_clip;
    bool have_path;

    have_path = !gx_path_is_void(ppath);
    if (!have_path) {
        if (!pdev->vg_initial_set) {
            /* See lib/gs_pdfwr.ps about "initial graphic state". */
            pdf_prepare_initial_viewer_state(pdev, pgs);
            pdf_reset_graphics(pdev);
            return 0;
        }
    }

    /* PostScript doesn't have a fill+stroke primitive, so break it into two operations
     * PDF 1.2 only has a single overprint setting, we can't be certainto match that
     * Because our inpu tcould be from a higher level. So be sure and break it into
     * 2 operations.
     */
    if (pdev->ForOPDFRead || pdev->CompatibilityLevel < 1.3) {
        code = gdev_pdf_fill_path(dev, pgs, ppath, fill_params, pdcolor_fill, pcpath);
        if (code < 0)
            return code;
        gs_swapcolors_quick(pgs);
        code = gdev_pdf_stroke_path(dev, pgs, ppath, stroke_params, pdcolor_stroke, pcpath);
        gs_swapcolors_quick(pgs);
        return code;
    } else {
        bool set_ctm;
        gs_matrix mat;
        double scale, path_scale;
        double prescale = 1;
        gs_fixed_rect bbox;
        gs_path_enum cenum;
        gdev_vector_dopath_state_t state;
        stream *s = pdev->strm;
        /*
         * Check for an empty clipping path.
         */
        if (pcpath) {
            gs_fixed_rect cbox;

            gx_cpath_outer_box(pcpath, &cbox);
            if (cbox.p.x >= cbox.q.x || cbox.p.y >= cbox.q.y)
                return 1;		/* empty clipping path */
        }
        code = pdf_check_soft_mask(pdev, (gs_gstate *)pgs);
        if (code < 0)
            return code;

        new_clip = pdf_must_put_clip_path(pdev, pcpath);
        if (have_path || pdev->context == PDF_IN_NONE || new_clip) {
            if (new_clip)
                code = pdf_unclip(pdev);
            else
                code = pdf_open_page(pdev, PDF_IN_STREAM);
            if (code < 0)
                return code;
        }
        code = pdf_prepare_fill_stroke(pdev, pgs, false);
        if (code < 0)
            return code;

        code = pdf_put_clip_path(pdev, pcpath);
        if (code < 0)
            return code;
        /*
         * If the CTM is not uniform, stroke width depends on angle.
         * We'd like to avoid resetting the CTM, so we check for uniform
         * CTMs explicitly.  Note that in PDF, unlike PostScript, it is
         * the CTM at the time of the stroke operation, not the CTM at
         * the time the path was constructed, that is used for transforming
         * the points of the path; so if we have to reset the CTM, we must
         * do it before constructing the path, and inverse-transform all
         * the coordinates.
         */
        set_ctm = (bool)gdev_vector_stroke_scaling((gx_device_vector *)pdev,
                                                   pgs, &scale, &mat);
        if (set_ctm && ((pgs->ctm.xx == 0 && pgs->ctm.xy == 0) ||
                        (pgs->ctm.yx == 0 && pgs->ctm.yy == 0))) {
            /* Acrobat Reader 5 and Adobe Reader 6 issues
               the "Wrong operand type" error with matrices, which have 3 zero coefs.
               Besides that, we found that Acrobat Reader 4, Acrobat Reader 5
               and Adobe Reader 6 all store the current path in user space
               and apply CTM in the time of stroking - See the bug 687901.
               Therefore a precise conversion of Postscript to PDF isn't possible in this case.
               Adobe viewers render a line with a constant width instead.
               At last, with set_ctm == true we need the inverse matrix in
               gdev_vector_dopath. Therefore we exclude projection matrices
               (see bug 688363). */
            set_ctm = false;
            scale = fabs(pgs->ctm.xx + pgs->ctm.xy + pgs->ctm.yx + pgs->ctm.yy) /* Using the non-zero coeff. */
                    / sqrt(2); /* Empirically from Adobe. */
        }
        if (pdev->PDFA == 1 && set_ctm) {
            /*
             * We want a scaling factor that will bring the largest reasonable
             * user coordinate within bounds.  We choose a factor based on the
             * minor axis of the transformation.  Thanks to Raph Levien for
             * the following formula.
             */
            double a = mat.xx, b = mat.xy, c = mat.yx, d = mat.yy;
            double u = fabs(a * d - b * c);
            double v = a * a + b * b + c * c + d * d;
            double minor = (sqrt(v + 2 * u) - sqrt(v - 2 * u)) * 0.5;

            prescale = (minor == 0 || minor > 1 ? 1 : 1 / minor);
        }
        gx_path_bbox(ppath, &bbox);
        {
            /* Check whether a painting appears inside the clipping box.
               Doing so after writing the clipping path due to /SP pdfmark
               uses a special hack with painting outside the clipping box
               for synchronizing the clipping path (see lib/gs_pdfwr.ps).
               That hack appeared because there is no way to pass
               the gs_gstate through gdev_pdf_put_params,
               which pdfmark is implemented with.
            */
            gs_fixed_rect clip_box, stroke_bbox = bbox;
            gs_point d0, d1;
            gs_fixed_point p0, p1;
            fixed bbox_expansion_x, bbox_expansion_y;

            gs_distance_transform(pgs->line_params.half_width, 0, &ctm_only(pgs), &d0);
            gs_distance_transform(0, pgs->line_params.half_width, &ctm_only(pgs), &d1);
            p0.x = float2fixed(any_abs(d0.x));
            p0.y = float2fixed(any_abs(d0.y));
            p1.x = float2fixed(any_abs(d1.x));
            p1.y = float2fixed(any_abs(d1.y));
            bbox_expansion_x = max(p0.x, p1.x) + fixed_1 * 2;
            bbox_expansion_y = max(p0.y, p1.y) + fixed_1 * 2;
            stroke_bbox.p.x -= bbox_expansion_x;
            stroke_bbox.p.y -= bbox_expansion_y;
            stroke_bbox.q.x += bbox_expansion_x;
            stroke_bbox.q.y += bbox_expansion_y;
            gx_cpath_outer_box(pcpath, &clip_box);
            rect_intersect(stroke_bbox, clip_box);
            if (stroke_bbox.q.x < stroke_bbox.p.x || stroke_bbox.q.y < stroke_bbox.p.y)
                return 0;
        }
        if (make_rect_scaling(pdev, &bbox, prescale, &path_scale)) {
            scale /= path_scale;
            if (set_ctm)
                gs_matrix_scale(&mat, path_scale, path_scale, &mat);
            else {
                gs_make_scaling(path_scale, path_scale, &mat);
                set_ctm = true;
            }
        }

        code = pdf_setfillcolor((gx_device_vector *)pdev, pgs, pdcolor_fill);
        if (code == gs_error_rangecheck) {
            /* rangecheck means we revert to the equivalent to the default implementation */
            code = gdev_pdf_fill_path(dev, pgs, ppath, fill_params, pdcolor_fill, pcpath);
            if (code < 0)
                return code;
            /* Swap colors to make sure the pgs colorspace is correct for stroke */
            gs_swapcolors_quick(pgs);
            code = gdev_pdf_stroke_path(dev, pgs, ppath, stroke_params, pdcolor_stroke, pcpath);
            gs_swapcolors_quick(pgs);
            return code;
        }

        /* Swap colors to make sure the pgs colorspace is correct for stroke */
        gs_swapcolors_quick(pgs);
        code = gdev_vector_prepare_stroke((gx_device_vector *)pdev, pgs, stroke_params,
                                          pdcolor_stroke, scale);
        gs_swapcolors_quick(pgs);
        if (code < 0) {
            code = gdev_pdf_fill_path(dev, pgs, ppath, fill_params, pdcolor_fill, pcpath);
            if (code < 0)
                return code;
            return gdev_pdf_stroke_path(dev, pgs, ppath, stroke_params, pdcolor_stroke, pcpath);
        }
        if (!pdev->HaveStrokeColor)
            pdev->saved_fill_color = pdev->saved_stroke_color;
        if (set_ctm)
            pdf_put_matrix(pdev, "q ", &mat, "cm\n");
        if (pgs->line_params.dash.offset != 0 || pgs->line_params.dash.pattern_size != 0)
            code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_stroke | gx_path_type_optimize | gx_path_type_dashed_stroke, (set_ctm ? &mat : (const gs_matrix *)0));
        else
            code = pdf_write_path(pdev, (gs_path_enum *)&cenum, &state, (gx_path *)ppath, 0, gx_path_type_stroke | gx_path_type_optimize, (set_ctm ? &mat : (const gs_matrix *)0));
        if (code < 0)
            return code;
        s = pdev->strm;
        stream_puts(s, (fill_params->rule < 0 ? "B\n" : "B*\n"));
        stream_puts(s, (set_ctm ? " Q\n" : "\n"));
    }
    return 0;
}

/*
   The fill_rectangle_hl_color device method.
   See gxdevcli.h about return codes.
 */
int
gdev_pdf_fill_rectangle_hl_color(gx_device *dev, const gs_fixed_rect *rect,
    const gs_gstate *pgs, const gx_drawing_color *pdcolor,
    const gx_clip_path *pcpath)
{
    int code;
    gs_fixed_rect box1 = *rect, box = box1;
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    double scale;
    gs_matrix smat, *psmat = NULL;
    const bool convert_to_image = (pdev->CompatibilityLevel <= 1.2 &&
            gx_dc_is_pattern2_color(pdcolor));

    if (rect->p.x == rect->q.x)
        return 0;
    if (!convert_to_image) {
        code = prepare_fill_with_clip(pdev, pgs, &box, true, pdcolor, pcpath);
        if (code < 0)
            return code;
        if (code == 1)
            return 0; /* Nothing to paint. */
        code = pdf_setfillcolor((gx_device_vector *)pdev, pgs, pdcolor);
        if (code < 0)
            return code;
        if (pcpath)
            rect_intersect(box1, box);
        if (box1.p.x > box1.q.x || box1.p.y > box1.q.y)
            return 0;		/* outside the clipping path */
        if (make_rect_scaling(pdev, &box1, 1.0, &scale)) {
            gs_make_scaling(pdev->scale.x * scale, pdev->scale.y * scale, &smat);
            pdf_put_matrix(pdev, "q ", &smat, "cm\n");
            psmat = &smat;
        }
        pprintg4(pdev->strm, "%g %g %g %g re f\n",
                fixed2float(box1.p.x) / scale, fixed2float(box1.p.y) / scale,
                fixed2float(box1.q.x - box1.p.x) / scale, fixed2float(box1.q.y - box1.p.y) / scale);
        if (psmat != NULL)
            stream_puts(pdev->strm, "Q\n");
        if (pdev->Eps2Write) {
            gs_rect *Box;

            if (!pdev->accumulating_charproc)
                Box = &pdev->BBox;
            else
                Box = &pdev->charproc_BBox;

            if (fixed2float(box1.p.x) / (pdev->HWResolution[0] / 72.0) < Box->p.x)
                Box->p.x = fixed2float(box1.p.x) / (pdev->HWResolution[0] / 72.0);
            if (fixed2float(box1.p.y) / (pdev->HWResolution[1] / 72.0) < Box->p.y)
                Box->p.y = fixed2float(box1.p.y) / (pdev->HWResolution[1] / 72.0);
            if (fixed2float(box1.q.x) / (pdev->HWResolution[0] / 72.0) > Box->q.x)
                Box->q.x = fixed2float(box1.q.x) / (pdev->HWResolution[0] / 72.0);
            if (fixed2float(box1.q.y) / (pdev->HWResolution[1] / 72.0) > Box->q.y)
                Box->q.y = fixed2float(box1.q.y) / (pdev->HWResolution[1] / 72.0);
        }
        return 0;
    } else {
        gx_fill_params params;
        gx_path path;

        params.rule = 1; /* Not important because the path is a rectange. */
        params.adjust.x = params.adjust.y = 0;
        params.flatness = pgs->flatness;
        gx_path_init_local(&path, pgs->memory);
        code = gx_path_add_rectangle(&path, rect->p.x, rect->p.y, rect->q.x, rect->q.y);
        if (code < 0)
            return code;
        code = gdev_pdf_fill_path(dev, pgs, &path, &params, pdcolor, pcpath);
        if (code < 0)
            return code;
        gx_path_free(&path, "gdev_pdf_fill_rectangle_hl_color");
        return code;

    }
}

int
gdev_pdf_fillpage(gx_device *dev, gs_gstate * pgs, gx_device_color *pdevc)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int bottom = (pdev->ResourcesBeforeUsage ? 1 : 0);

    if (gx_dc_pure_color(pdevc) == pdev->white && !is_in_page(pdev) && pdev->sbstack_depth <= bottom) {
        /* PDF doesn't need to erase the page if its plain white */
        return 0;
    }
    else
        return gx_default_fillpage(dev, pgs, pdevc);
}
