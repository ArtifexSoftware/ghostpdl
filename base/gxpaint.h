/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Internal interface to fill/stroke */
/* Requires gsropt.h, gxfixed.h, gxpath.h */

#ifndef gxpaint_INCLUDED
#  define gxpaint_INCLUDED

#include "gxpath.h"
#include "gxfixed.h"
#include "gsdevice.h"
#include "gsdcolor.h"

/* ------ Graphics-state-aware procedures ------ */

/*
 * The following procedures use information from the graphics state.
 * They are implemented in gxpaint.c.
 */

int gx_fill_path(gx_path * ppath, gx_device_color * pdevc, gs_gstate * pgs,
                 int rule, fixed adjust_x, fixed adjust_y);
int gx_stroke_fill(gx_path * ppath, gs_gstate * pgs);
int gx_stroke_add(gx_path *ppath, gx_path *to_path, const gs_gstate * pgs, bool traditional);
int gx_fill_stroke_path(gs_gstate *pgs, int rule);

/*
 * gx_gstate_stroke_add needs a device for the sake of absolute-length
 * dots (and for no other reason).
 */
int gx_gstate_stroke_add(gx_path *ppath, gx_path *to_path,
                         gx_device *dev, const gs_gstate *pgs);

/* ------ Imager procedures ------ */

/*
 * Tweak the fill adjustment if necessary so that (nearly) empty
 * rectangles are guaranteed to produce some output.
 */
void gx_adjust_if_empty(const gs_fixed_rect *, gs_fixed_point *);

/*
 * Compute the amount by which to expand a stroked bounding box to account
 * for line width, caps and joins.  If the amount is too large to fit in a
 * gs_fixed_point, return gs_error_limitcheck.  Return 0 if the result is
 * exact, 1 if it is conservative.
 *
 * This procedure is fast, but the result may be conservative by a large
 * amount if the miter limit is large.  If this matters, use strokepath +
 * pathbbox.
 */
int gx_stroke_path_expansion(const gs_gstate *pgs,
                             const gx_path *ppath, gs_fixed_point *ppt);

/* Backward compatibility */
#define gx_stroke_expansion(pgs, ppt)\
  gx_stroke_path_expansion(pgs, (const gx_path *)0, ppt)

/*
 * The following procedures do not need a graphics state.
 * These procedures are implemented in gxfill.c and gxstroke.c.
 */

/* Define the parameters passed to the imager's filling routine. */
struct gx_fill_params_s {
    int rule;			/* -1 = winding #, 1 = even/odd */
    gs_fixed_point adjust;
    float flatness;
};

#define gx_fill_path_only(ppath, dev, pgs, params, pdevc, pcpath)\
  (*dev_proc(dev, fill_path))(dev, pgs, ppath, params, pdevc, pcpath)

/* Define the parameters passed to the imager's stroke routine. */
struct gx_stroke_params_s {
    float flatness;
    bool  traditional;
};

int gx_stroke_path_only(gx_path * ppath, gx_path * to_path, gx_device * dev,
                        const gs_gstate * pgs,
                        const gx_stroke_params * params,
                        const gx_device_color * pdevc,
                        const gx_clip_path * pcpath);

#endif /* gxpaint_INCLUDED */
