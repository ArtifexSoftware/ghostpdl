/* Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*Id: gdevpdfd.c  */
/* Path drawing procedures for pdfwrite driver */
#include "math_.h"
#include "gx.h"
#include "gxdevice.h"
#include "gxfixed.h"
#include "gxistate.h"
#include "gxpaint.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gdevpdfx.h"

/* ---------------- Drawing ---------------- */

/* Fill a rectangle. */
int
gdev_pdf_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
			gx_color_index color)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    int code;

    /* Make a special check for the initial fill with white, */
    /* which shouldn't cause the page to be opened. */
    if (color == 0xffffff && !is_in_page(pdev))
	return 0;
    code = pdf_open_page(pdev, pdf_in_stream);
    if (code < 0)
	return code;
    /* Make sure we aren't being clipped. */
    pdf_put_clip_path(pdev, NULL);
    pdf_set_color(pdev, color, &pdev->fill_color, "rg");
    pprintd4(pdev->strm, "%d %d %d %d re f\n", x, y, w, h);
    return 0;
}

/* ---------------- Path drawing ---------------- */

/* ------ Utilities ------ */

/*
 * Put a segment of an enumerated path on the output file.
 * pe_op is assumed to be valid.
 */
private int
pdf_put_path_segment(gx_device_pdf * pdev, int pe_op, gs_fixed_point vs[3],
		     const gs_matrix * pmat)
{
    stream *s = pdev->strm;
    const char *format;
    gs_point vp[3];

    switch (pe_op) {
	case gs_pe_moveto:
	    format = "%g %g m\n";
	    goto do1;
	case gs_pe_lineto:
	    format = "%g %g l\n";
	  do1:vp[0].x = fixed2float(vs[0].x), vp[0].y = fixed2float(vs[0].y);
	    if (pmat)
		gs_point_transform_inverse(vp[0].x, vp[0].y, pmat, &vp[0]);
	    pprintg2(s, format, vp[0].x, vp[0].y);
	    break;
	case gs_pe_curveto:
	    vp[0].x = fixed2float(vs[0].x), vp[0].y = fixed2float(vs[0].y);
	    vp[1].x = fixed2float(vs[1].x), vp[1].y = fixed2float(vs[1].y);
	    vp[2].x = fixed2float(vs[2].x), vp[2].y = fixed2float(vs[2].y);
	    if (pmat) {
		gs_point_transform_inverse(vp[0].x, vp[0].y, pmat, &vp[0]);
		gs_point_transform_inverse(vp[1].x, vp[1].y, pmat, &vp[1]);
		gs_point_transform_inverse(vp[2].x, vp[2].y, pmat, &vp[2]);
	    }
	    pprintg6(s, "%g %g %g %g %g %g c\n",
		     vp[0].x, vp[0].y, vp[1].x, vp[1].y, vp[2].x, vp[2].y);
	    break;
	case gs_pe_closepath:
	    pputs(s, "h\n");
	    break;
	default:		/* can't happen */
	    return -1;
    }
    return 0;
}

/* Put a path on the output file.  If do_close is false and the last */
/* path component is a closepath, omit it and return 1. */
private int
pdf_put_path(gx_device_pdf * pdev, const gx_path * ppath, bool do_close,
	     const gs_matrix * pmat)
{
    stream *s = pdev->strm;
    gs_fixed_rect rbox;
    gx_path_rectangular_type rtype = gx_path_is_rectangular(ppath, &rbox);
    gs_path_enum cenum;

    /*
     * If do_close is false (fill), we recognize all rectangles;
     * if do_close is true (stroke), we only recognize closed
     * rectangles.
     */
    if (rtype != prt_none &&
	!(do_close && rtype == prt_open) &&
	(pmat == 0 || is_xxyy(pmat) || is_xyyx(pmat))
	) {
	gs_point p, q;

	p.x = fixed2float(rbox.p.x), p.y = fixed2float(rbox.p.y);
	q.x = fixed2float(rbox.q.x), q.y = fixed2float(rbox.q.y);
	if (pmat) {
	    gs_point_transform_inverse(p.x, p.y, pmat, &p);
	    gs_point_transform_inverse(q.x, q.y, pmat, &q);
	}
	pprintg4(s, "%g %g %g %g re\n",
		 p.x, p.y, q.x - p.x, q.y - p.y);
	return 0;
    }
    gx_path_enum_init(&cenum, ppath);
    for (;;) {
	gs_fixed_point vs[3];
	int pe_op = gx_path_enum_next(&cenum, vs);

      sw:switch (pe_op) {
	    case 0:		/* done */
		return 0;
	    case gs_pe_closepath:
		if (!do_close) {
		    pe_op = gx_path_enum_next(&cenum, vs);
		    if (pe_op != 0) {
			pputs(s, "h\n");
			goto sw;
		    }
		    return 1;
		}
		/* falls through */
	    default:
		pdf_put_path_segment(pdev, pe_op, vs, pmat);
	}
    }
}

/* Test whether we will need to put the clipping path. */
bool
pdf_must_put_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath)
{
    if (pcpath == NULL)
	return pdev->clip_path_id != pdev->no_clip_path_id;
    if (pdev->clip_path_id == pcpath->id)
	return false;
    if (gx_cpath_includes_rectangle(pcpath, fixed_0, fixed_0,
				    int2fixed(pdev->width),
				    int2fixed(pdev->height))
	)
	return pdev->clip_path_id != pdev->no_clip_path_id;
    return true;
}

/* Put a clipping path on the output file. */
int
pdf_put_clip_path(gx_device_pdf * pdev, const gx_clip_path * pcpath)
{
    stream *s = pdev->strm;

    if (pcpath == NULL) {
	if (pdev->clip_path_id == pdev->no_clip_path_id)
	    return 0;
	pputs(s, "Q\nq\n");
	pdev->clip_path_id = pdev->no_clip_path_id;
    } else {
	if (pdev->clip_path_id == pcpath->id)
	    return 0;
	if (gx_cpath_includes_rectangle(pcpath, fixed_0, fixed_0,
					int2fixed(pdev->width),
					int2fixed(pdev->height))
	    ) {
	    if (pdev->clip_path_id == pdev->no_clip_path_id)
		return 0;
	    pputs(s, "Q\nq\n");
	    pdev->clip_path_id = pdev->no_clip_path_id;
	} else {
	    gs_cpath_enum cenum;
	    gs_fixed_point vs[3];
	    int pe_op;

	    pputs(s, "Q\nq\nW\n");
	    /*
	     * We have to break 'const' here because the clip path
	     * enumeration logic uses some internal mark bits.
	     * This is very unfortunate, but until we can come up with
	     * a better algorithm, it's necessary.
	     */
	    gx_cpath_enum_init(&cenum, (gx_clip_path *) pcpath);
	    while ((pe_op = gx_cpath_enum_next(&cenum, vs)) > 0)
		pdf_put_path_segment(pdev, pe_op, vs, NULL);
	    pputs(s, "n\n");
	    if (pe_op < 0)
		return pe_op;
	    pdev->clip_path_id = pcpath->id;
	}
    }
    pdev->text.font = 0;
    if (pdev->context == pdf_in_text)
	pdev->context = pdf_in_stream;
    pdf_reset_graphics(pdev);
    return 0;
}

/* ------ Driver procedures ------ */

/* Fill a path. */
int
gdev_pdf_fill_path(gx_device * dev, const gs_imager_state * pis, gx_path * ppath,
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

    /*
     * Check for an empty clipping path.
     */
    if (pcpath) {
	gs_fixed_rect box;

	gx_cpath_outer_box(pcpath, &box);
	if (box.p.x >= box.q.x || box.p.y >= box.q.y)
	    return 0;		/* empty clipping path */
    }
    if (!gx_dc_is_pure(pdcolor))
	return gx_default_fill_path(dev, pis, ppath, params, pdcolor,
				    pcpath);
    /*
     * Make a special check for the initial fill with white,
     * which shouldn't cause the page to be opened.
     */
    if (gx_dc_pure_color(pdcolor) == 0xffffff && !is_in_page(pdev))
	return 0;
    have_path = !gx_path_is_void(ppath);
    if (have_path || pdev->context == pdf_in_none ||
	pdf_must_put_clip_path(pdev, pcpath)
	) {
	code = pdf_open_page(pdev, pdf_in_stream);
	if (code < 0)
	    return code;
    }
    pdf_put_clip_path(pdev, pcpath);
    pdf_set_color(pdev, gx_dc_pure_color(pdcolor), &pdev->fill_color, "rg");
    if (have_path) {
	stream *s = pdev->strm;

	if (params->flatness != pdev->flatness) {
	    pprintg1(s, "%g i\n", params->flatness);
	    pdev->flatness = params->flatness;
	}
	pdf_put_path(pdev, ppath, false, NULL);
	pputs(s, (params->rule < 0 ? "f\n" : "f*\n"));
    }
    return 0;
}

/* Compare two dash patterns. */
private bool
pdf_dash_pattern_eq(const float *stored, const gx_dash_params * set, float scale)
{
    int i;

    for (i = 0; i < set->pattern_size; ++i)
	if (stored[i] != (float)(set->pattern[i] * scale))
	    return false;
    return true;
}

/* Stroke a path. */
int
gdev_pdf_stroke_path(gx_device * dev, const gs_imager_state * pis,
		     gx_path * ppath, const gx_stroke_params * params,
	      const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    gx_device_pdf *pdev = (gx_device_pdf *) dev;
    stream *s;
    int code;
    int pattern_size = pis->line_params.dash.pattern_size;
    double scale;
    bool set_ctm;
    gs_matrix mat;
    const gs_matrix *pmat;
    int i;

    if (gx_path_is_void(ppath))
	return 0;		/* won't mark the page */
    if (!gx_dc_is_pure(pdcolor))
	return gx_default_stroke_path(dev, pis, ppath, params, pdcolor,
				      pcpath);
    code = pdf_open_page(pdev, pdf_in_stream);
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
    if (pis->ctm.xy == 0 && pis->ctm.yx == 0) {
	scale = fabs(pis->ctm.xx);
	set_ctm = fabs(pis->ctm.yy) != scale;
    } else if (pis->ctm.xx == 0 && pis->ctm.yy == 0) {
	scale = fabs(pis->ctm.xy);
	set_ctm = fabs(pis->ctm.yx) != scale;
    } else if ((pis->ctm.xx == pis->ctm.yy && pis->ctm.xy == -pis->ctm.yx) ||
	       (pis->ctm.xx == -pis->ctm.yy && pis->ctm.xy == pis->ctm.yx)
	) {
	scale = hypot(pis->ctm.xx, pis->ctm.xy);
	set_ctm = false;
    } else
	set_ctm = true;
    if (set_ctm) {
	scale = 1;
	mat.xx = pis->ctm.xx / pdev->scale.x;
	mat.xy = pis->ctm.xy / pdev->scale.y;
	mat.yx = pis->ctm.yx / pdev->scale.x;
	mat.yy = pis->ctm.yy / pdev->scale.y;
	mat.tx = mat.ty = 0;
	pmat = &mat;
    } else {
	pmat = 0;
    }

    pdf_put_clip_path(pdev, pcpath);
    pdf_set_color(pdev, gx_dc_pure_color(pdcolor), &pdev->stroke_color, "RG");
    s = pdev->strm;
    if ((float)(pis->line_params.dash.offset * scale) != pdev->line_params.dash.offset ||
	pattern_size != pdev->line_params.dash.pattern_size ||
	pattern_size > max_dash ||
	(pattern_size != 0 &&
	 !pdf_dash_pattern_eq(pdev->dash_pattern, &pis->line_params.dash,
			      scale))
	) {
	pputs(s, "[ ");
	pdev->line_params.dash.pattern_size = pattern_size;
	for (i = 0; i < pattern_size; ++i) {
	    float element = pis->line_params.dash.pattern[i] * scale;

	    if (i < max_dash)
		pdev->dash_pattern[i] = element;
	    pprintg1(s, "%g ", element);
	}
	pdev->line_params.dash.offset =
	    pis->line_params.dash.offset * scale;
	pprintg1(s, "] %g d\n", pdev->line_params.dash.offset);
    }
    if (params->flatness != pdev->flatness) {
	pprintg1(s, "%g i\n", params->flatness);
	pdev->flatness = params->flatness;
    }
    if ((float)(pis->line_params.half_width * scale) != pdev->line_params.half_width) {
	pdev->line_params.half_width = pis->line_params.half_width * scale;
	pprintg1(s, "%g w\n", pdev->line_params.half_width * 2);
    }
    if (pis->line_params.miter_limit != pdev->line_params.miter_limit) {
	pprintg1(s, "%g M\n", pis->line_params.miter_limit);
	gx_set_miter_limit(&pdev->line_params,
			   pis->line_params.miter_limit);
    }
    if (pis->line_params.cap != pdev->line_params.cap) {
	pprintd1(s, "%d J\n", pis->line_params.cap);
	pdev->line_params.cap = pis->line_params.cap;
    }
    if (pis->line_params.join != pdev->line_params.join) {
	pprintd1(s, "%d j\n", pis->line_params.join);
	pdev->line_params.join = pis->line_params.join;
    }
    if (set_ctm)
	pdf_put_matrix(pdev, "q ", &mat, "cm\n");
    code = pdf_put_path(pdev, ppath, true, pmat);
    if (code < 0)
	return code;
    pputs(s, (code ? "s" : "S"));
    pputs(s, (set_ctm ? " Q\n" : "\n"));
    return 0;
}
