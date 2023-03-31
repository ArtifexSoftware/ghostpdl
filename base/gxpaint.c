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


/* Graphics-state-aware fill and stroke procedures */
#include "gx.h"
#include "gzstate.h"
#include "gxdevice.h"
#include "gxhttile.h"
#include "gxpaint.h"
#include "gxpath.h"
#include "gxfont.h"

static bool caching_an_outline_font(const gs_gstate * pgs)
{
    return pgs->in_cachedevice > 1 &&
            pgs->font != NULL &&
            pgs->font->FontType != ft_user_defined &&
            pgs->font->FontType != ft_PDF_user_defined &&
            pgs->font->FontType != ft_PCL_user_defined &&
            pgs->font->FontType != ft_GL2_stick_user_defined &&
            pgs->font->FontType != ft_CID_user_defined;
}

/* Fill a path. */
int
gx_fill_path(gx_path * ppath, gx_device_color * pdevc, gs_gstate * pgs,
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
    params.flatness = (caching_an_outline_font(pgs) ? 0.0 : pgs->flatness);
    return (*dev_proc(dev, fill_path))
        (dev, (const gs_gstate *)pgs, ppath, &params, pdevc, pcpath);
}

/* Stroke a path for drawing or saving. */
int
gx_stroke_fill(gx_path * ppath, gs_gstate * pgs)
{
    gx_device *dev = gs_currentdevice_inline(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);
    gx_stroke_params params;

    if (code < 0)
        return code;
    params.flatness = (caching_an_outline_font(pgs) ? 0.0 : pgs->flatness);
    params.traditional = false;

    code = (*dev_proc(dev, stroke_path))
        (dev, (const gs_gstate *)pgs, ppath, &params,
         gs_currentdevicecolor_inline(pgs), pcpath);

    if (pgs->black_textvec_state) {
        gsicc_restore_blacktextvec(pgs, true);
    }

    return code;
}

int
gx_fill_stroke_path(gs_gstate * pgs, int rule)
{
    gx_device *dev = gs_currentdevice_inline(pgs);
    gx_clip_path *pcpath;
    int code = gx_effective_clip_path(pgs, &pcpath);
    gx_stroke_params stroke_params;
    gx_fill_params fill_params;

    if (code < 0)
        return code;
    fill_params.rule = rule;
    fill_params.adjust.x = pgs->fill_adjust.x;
    fill_params.adjust.y = pgs->fill_adjust.y;
    fill_params.flatness = (caching_an_outline_font(pgs) ? 0.0 : pgs->flatness);
    stroke_params.flatness = (caching_an_outline_font(pgs) ? 0.0 : pgs->flatness);
    stroke_params.traditional = false;

    code = (*dev_proc(dev, fill_stroke_path))
        (dev, (const gs_gstate *)pgs, pgs->path,
         &fill_params, gs_currentdevicecolor_inline(pgs),
         &stroke_params, gs_swappeddevicecolor_inline(pgs),
         pcpath);

    if (pgs->black_textvec_state) {
        gsicc_restore_blacktextvec(pgs, true);
    }

    return code;
}

int
gx_stroke_add(gx_path * ppath, gx_path * to_path,
              const gs_gstate * pgs, bool traditional)
{
    gx_stroke_params params;

    params.flatness = (caching_an_outline_font(pgs) ? 0.0 : pgs->flatness);
    params.traditional = traditional;
    return gx_stroke_path_only(ppath, to_path, pgs->device,
                               (const gs_gstate *)pgs,
                               &params, NULL, NULL);
}

int
gx_gstate_stroke_add(gx_path *ppath, gx_path *to_path,
                     gx_device *dev, const gs_gstate *pgs)
{
    gx_stroke_params params;

    params.flatness = pgs->flatness;
    params.traditional = false;
    return gx_stroke_path_only(ppath, to_path, dev, pgs,
                               &params, NULL, NULL);
}
