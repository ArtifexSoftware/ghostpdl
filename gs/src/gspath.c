/* Copyright (C) 1989, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.

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

/* gspath.c */
/* Basic path routines for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gscoord.h"		/* requires gsmatrix.h */
#include "gzstate.h"
#include "gzpath.h"
#include "gxdevice.h"		/* for gxcpath.h */
#include "gzcpath.h"

/* ------ Miscellaneous ------ */

int
gs_newpath(gs_state * pgs)
{
    gx_path_release(pgs->path);
    gx_path_init(pgs->path, pgs->memory);
    return 0;
}

int
gs_closepath(gs_state * pgs)
{
    gx_path *ppath = pgs->path;
    int code = gx_path_close_subpath(ppath);

    if (code < 0)
	return code;
    if (path_start_outside_range(ppath))
	path_set_outside_position(ppath, ppath->outside_start.x,
				  ppath->outside_start.y);
    return code;
}

int
gs_upmergepath(gs_state * pgs)
{
    return gx_path_add_path(pgs->saved->path, pgs->path);
}

/* Get the current path (for internal use only). */
gx_path *
gx_current_path(const gs_state * pgs)
{
    return pgs->path;
}

/* ------ Points and lines ------ */

/*
 * Define clamped values for out-of-range coordinates.
 * Currently the path drawing routines can't handle values
 * close to the edge of the representable space.
 */
#define max_coord_fixed (max_fixed - int2fixed(1000))	/* arbitrary */
#define min_coord_fixed (-max_coord_fixed)
private void
clamp_point(gs_fixed_point * ppt, floatp x, floatp y)
{
#define clamp_coord(xy)\
  ppt->xy = (xy > fixed2float(max_coord_fixed) ? max_coord_fixed :\
	     xy < fixed2float(min_coord_fixed) ? min_coord_fixed :\
	     float2fixed(xy))
    clamp_coord(x);
    clamp_coord(y);
#undef clamp_coord
}

int
gs_currentpoint(const gs_state * pgs, gs_point * ppt)
{
    gx_path *ppath = pgs->path;
    int code;
    gs_fixed_point pt;

    if (path_outside_range(ppath))
	return gs_itransform((gs_state *) pgs,
			     ppath->outside_position.x,
			     ppath->outside_position.y, ppt);
    code = gx_path_current_point(pgs->path, &pt);
    if (code < 0)
	return code;
    return gs_itransform((gs_state *) pgs,
			 fixed2float(pt.x), fixed2float(pt.y), ppt);
}

int
gs_moveto(gs_state * pgs, floatp x, floatp y)
{
    gx_path *ppath = pgs->path;
    gs_fixed_point pt;
    int code;

    if ((code = gs_point_transform2fixed(&pgs->ctm, x, y, &pt)) < 0) {
	if (pgs->clamp_coordinates) {	/* Handle out-of-range coordinates. */
	    gs_point opt;

	    if (code != gs_error_limitcheck ||
		(code = gs_transform(pgs, x, y, &opt)) < 0
		)
		return code;
	    clamp_point(&pt, opt.x, opt.y);
	    code = gx_path_add_point(ppath, pt.x, pt.y);
	    if (code < 0)
		return code;
	    path_set_outside_position(ppath, opt.x, opt.y);
	    ppath->outside_start = ppath->outside_position;
	    ppath->start_flags = ppath->state_flags;
	}
	return code;
    }
    return gx_path_add_point(ppath, pt.x, pt.y);
}

int
gs_rmoveto(gs_state * pgs, floatp x, floatp y)
{
    gs_fixed_point dpt;
    int code;

    if ((code = gs_distance_transform2fixed(&pgs->ctm, x, y, &dpt)) < 0 ||
	(code = gx_path_add_relative_point(pgs->path, dpt.x, dpt.y)) < 0
	) {			/* Handle all exceptional conditions here. */
	gs_point upt;

	if ((code = gs_currentpoint(pgs, &upt)) < 0)
	    return code;
	return gs_moveto(pgs, upt.x + x, upt.y + y);
    }
    return code;
}

int
gs_lineto(gs_state * pgs, floatp x, floatp y)
{
    gx_path *ppath = pgs->path;
    int code;
    gs_fixed_point pt;

    if ((code = gs_point_transform2fixed(&pgs->ctm, x, y, &pt)) < 0) {
	if (pgs->clamp_coordinates) {	/* Handle out-of-range coordinates. */
	    gs_point opt;

	    if (code != gs_error_limitcheck ||
		(code = gs_transform(pgs, x, y, &opt)) < 0
		)
		return code;
	    clamp_point(&pt, opt.x, opt.y);
	    code = gx_path_add_line(ppath, pt.x, pt.y);
	    if (code < 0)
		return code;
	    path_set_outside_position(ppath, opt.x, opt.y);
	}
	return code;
    }
    return gx_path_add_line(pgs->path, pt.x, pt.y);
}

int
gs_rlineto(gs_state * pgs, floatp x, floatp y)
{
    gx_path *ppath = pgs->path;
    gs_fixed_point dpt;
    fixed nx, ny;
    int code;

    if (!path_position_in_range(ppath) ||
	(code = gs_distance_transform2fixed(&pgs->ctm, x, y, &dpt)) < 0 ||
    /* Check for overflow in addition. */
	(((nx = ppath->position.x + dpt.x) ^ dpt.x) < 0 &&
	 (ppath->position.x ^ dpt.x) >= 0) ||
	(((ny = ppath->position.y + dpt.y) ^ dpt.y) < 0 &&
	 (ppath->position.y ^ dpt.y) >= 0) ||
	(code = gx_path_add_line(ppath, nx, ny)) < 0
	) {			/* Handle all exceptional conditions here. */
	gs_point upt;

	if ((code = gs_currentpoint(pgs, &upt)) < 0)
	    return code;
	return gs_lineto(pgs, upt.x + x, upt.y + y);
    }
    return code;
}

/* ------ Curves ------ */

int
gs_curveto(gs_state * pgs,
	   floatp x1, floatp y1, floatp x2, floatp y2, floatp x3, floatp y3)
{
    gs_fixed_point p1, p2, p3;
    int code1 = gs_point_transform2fixed(&pgs->ctm, x1, y1, &p1);
    int code2 = gs_point_transform2fixed(&pgs->ctm, x2, y2, &p2);
    int code3 = gs_point_transform2fixed(&pgs->ctm, x3, y3, &p3);
    gx_path *ppath = pgs->path;

    if ((code1 | code2 | code3) < 0) {
	if (pgs->clamp_coordinates) {	/* Handle out-of-range coordinates. */
	    gs_point opt1, opt2, opt3;
	    int code;

	    if ((code1 < 0 && code1 != gs_error_limitcheck) ||
		(code1 = gs_transform(pgs, x1, y1, &opt1)) < 0
		)
		return code1;
	    if ((code2 < 0 && code2 != gs_error_limitcheck) ||
		(code2 = gs_transform(pgs, x2, y2, &opt2)) < 0
		)
		return code2;
	    if ((code3 < 0 && code3 != gs_error_limitcheck) ||
		(code3 = gs_transform(pgs, x3, y3, &opt3)) < 0
		)
		return code3;
	    clamp_point(&p1, opt1.x, opt1.y);
	    clamp_point(&p2, opt2.x, opt2.y);
	    clamp_point(&p3, opt3.x, opt3.y);
	    code = gx_path_add_curve(ppath,
				     p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	    if (code < 0)
		return code;
	    path_set_outside_position(ppath, opt3.x, opt3.y);
	    return code;
	} else
	    return (code1 < 0 ? code1 : code2 < 0 ? code2 : code3);
    }
    return gx_path_add_curve(ppath,
			     p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
}

int
gs_rcurveto(gs_state * pgs,
     floatp dx1, floatp dy1, floatp dx2, floatp dy2, floatp dx3, floatp dy3)
{
    gx_path *ppath = pgs->path;
    gs_fixed_point p1, p2, p3;
    fixed ptx, pty;
    int code;

/****** SHOULD CHECK FOR OVERFLOW IN ADDITION ******/
    if (!path_position_in_range(ppath) ||
	(code = gs_distance_transform2fixed(&pgs->ctm, dx1, dy1, &p1)) < 0 ||
	(code = gs_distance_transform2fixed(&pgs->ctm, dx2, dy2, &p2)) < 0 ||
	(code = gs_distance_transform2fixed(&pgs->ctm, dx3, dy3, &p3)) < 0 ||
	(ptx = ppath->position.x, pty = ppath->position.y,
	 code = gx_path_add_curve(ppath, ptx + p1.x, pty + p1.y,
				  ptx + p2.x, pty + p2.y,
				  ptx + p3.x, pty + p3.y)) < 0
	) {			/* Handle all exceptional conditions here. */
	gs_point upt;

	if ((code = gs_currentpoint(pgs, &upt)) < 0)
	    return code;
	return gs_curveto(pgs, upt.x + dx1, upt.y + dy1,
			  upt.x + dx2, upt.y + dy2,
			  upt.x + dx3, upt.y + dy3);
    }
    return code;
}

/* ------ Clipping ------ */

/* Forward references */
private int common_clip(P2(gs_state *, int));
private int set_clip_path(P2(gs_state *, gx_clip_path *));

int
gs_clippath(gs_state * pgs)
{
    gx_path path, cpath;
    int code = gx_cpath_path(pgs->clip_path, &path);

    if (code < 0)
	return code;
    code = gx_path_copy(&path, &cpath, 1);
    if (code < 0)
	return code;
    gx_path_release(pgs->path);
    *pgs->path = cpath;
    return 0;
}

int
gs_initclip(gs_state * pgs)
{
    register gx_device *dev = gs_currentdevice(pgs);
    gs_rect bbox;
    gs_fixed_rect box;
    gs_matrix imat;

    if (dev->ImagingBBox_set) {	/* Use the ImagingBBox, relative to default user space. */
	gs_defaultmatrix(pgs, &imat);
	bbox.p.x = dev->ImagingBBox[0];
	bbox.p.y = dev->ImagingBBox[1];
	bbox.q.x = dev->ImagingBBox[2];
	bbox.q.y = dev->ImagingBBox[3];
    } else {			/* Use the MediaSize indented by the HWMargins, */
	/* relative to unrotated user space adjusted by */
	/* the Margins.  (We suspect this isn't quite right, */
	/* but the whole issue of "margins" is such a mess that */
	/* we don't think we can do any better.) */
	(*dev_proc(dev, get_initial_matrix)) (dev, &imat);
	/* Adjust for the Margins. */
	imat.tx += dev->Margins[0] * dev->HWResolution[0] /
	    dev->MarginsHWResolution[0];
	imat.ty += dev->Margins[1] * dev->HWResolution[1] /
	    dev->MarginsHWResolution[1];
	bbox.p.x = dev->HWMargins[0];
	bbox.p.y = dev->HWMargins[1];
	bbox.q.x = dev->MediaSize[0] - dev->HWMargins[2];
	bbox.q.y = dev->MediaSize[1] - dev->HWMargins[3];
    }
    gs_bbox_transform(&bbox, &imat, &bbox);
    /* Round the clipping box so that it doesn't get ceilinged. */
    box.p.x = fixed_rounded(float2fixed(bbox.p.x));
    box.p.y = fixed_rounded(float2fixed(bbox.p.y));
    box.q.x = fixed_rounded(float2fixed(bbox.q.x));
    box.q.y = fixed_rounded(float2fixed(bbox.q.y));
    return gx_clip_to_rectangle(pgs, &box);
}

int
gs_clip(gs_state * pgs)
{
    return common_clip(pgs, gx_rule_winding_number);
}

int
gs_eoclip(gs_state * pgs)
{
    return common_clip(pgs, gx_rule_even_odd);
}

private int
common_clip(gs_state * pgs, int rule)
{
    gx_path fpath;
    int code = gx_path_flatten_accurate(pgs->path, &fpath, pgs->flatness,
					pgs->accurate_curves);

    if (code < 0)
	return code;
    code = gx_cpath_intersect(pgs, pgs->clip_path, &fpath, rule);
    if (code != 1)
	gx_path_release(&fpath);
    if (code < 0)
	return code;
    pgs->clip_path->rule = rule;
    return set_clip_path(pgs, pgs->clip_path);
}

int
gs_setclipoutside(gs_state * pgs, bool outside)
{
    return gx_cpath_set_outside(pgs->clip_path, outside);
}

bool
gs_currentclipoutside(const gs_state * pgs)
{
    return gx_cpath_is_outside(pgs->clip_path);
}

/* Establish a rectangle as the clipping path. */
/* Used by initclip and by the character and Pattern cache logic. */
int
gx_clip_to_rectangle(gs_state * pgs, gs_fixed_rect * pbox)
{
    gx_clip_path cpath;
    int code = gx_cpath_from_rectangle(&cpath, pbox, pgs->memory);

    if (code < 0)
	return code;
    gx_cpath_release(pgs->clip_path);
    cpath.rule = gx_rule_winding_number;
    return set_clip_path(pgs, &cpath);
}

/* Set the clipping path to the current path, without intersecting. */
/* Currently only used by the insideness testing operators, */
/* but might be used by viewclip eventually. */
/* The algorithm is very inefficient; we'll improve it later if needed. */
int
gx_clip_to_path(gs_state * pgs)
{
    gs_fixed_rect bbox;
    int code;

    if ((code = gx_path_bbox(pgs->path, &bbox)) < 0 ||
	(code = gx_clip_to_rectangle(pgs, &bbox)) < 0
	)
	return code;
    return gs_clip(pgs);
}

/* Set the clipping path (internal). */
private int
set_clip_path(gs_state * pgs, gx_clip_path * pcpath)
{
    *pgs->clip_path = *pcpath;
#ifdef DEBUG
    if (gs_debug_c('P')) {
	extern void gx_cpath_print(P1(const gx_clip_path *));

	dprintf("[P]Clipping path:\n"),
	    gx_cpath_print(pcpath);
    }
#endif
    return 0;
}
