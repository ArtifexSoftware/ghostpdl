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

/*$RCSfile$ $Revision$ */
/* Graphics-state-aware fill and stroke procedures */
#include "gx.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxhttile.h"
#include "gxpaint.h"
#include "gxpath.h"

/* Fill a path. */
int
gx_fill_path(gx_path * ppath, gx_device_color * pdevc, gs_state * pgs,
	     int rule, fixed adjust_x, fixed adjust_y)
{
    gx_device *dev = gs_currentdevice_inline(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);
    gx_fill_params params;

    if (code < 0)
	return code;
    params.rule = rule;
    params.adjust.x = adjust_x;
    params.adjust.y = adjust_y;
    params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
    params.fill_zero_width = (adjust_x | adjust_y) != 0;
    return (*dev_proc(dev, fill_path))
	(dev, (const gs_imager_state *)pgs, ppath, &params, pdevc, pcpath);
}

/* Stroke a path for drawing or saving. */
int
gx_stroke_fill(gx_path * ppath, gs_state * pgs)
{
    gx_device *dev = gs_currentdevice_inline(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);
    gx_stroke_params params;

    if (code < 0)
	return code;
    params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
    return (*dev_proc(dev, stroke_path))
	(dev, (const gs_imager_state *)pgs, ppath, &params,
	 pgs->dev_color, pcpath);
}

int
gx_stroke_add(gx_path * ppath, gx_path * to_path,
	      const gs_state * pgs)
{
    gx_stroke_params params;

    params.flatness = (pgs->in_cachedevice > 1 ? 0.0 : pgs->flatness);
    return gx_stroke_path_only(ppath, to_path, pgs->device,
			       (const gs_imager_state *)pgs,
			       &params, NULL, NULL);
}

int
gx_imager_stroke_add(gx_path *ppath, gx_path *to_path,
		     gx_device *dev, const gs_imager_state *pis)
{
    gx_stroke_params params;

    params.flatness = pis->flatness;
    return gx_stroke_path_only(ppath, to_path, dev, pis,
			       &params, NULL, NULL);
}
