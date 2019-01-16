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


/* XPS interpreter - transparency support */

#include "ghostxps.h"

void
xps_bounds_in_user_space(xps_context_t *ctx, gs_rect *ubox)
{
    gx_clip_path *clip_path;
    gs_rect dbox;
    int code;

    code = gx_effective_clip_path(ctx->pgs, &clip_path);
    if (code < 0)
        gs_warn("gx_effective_clip_path failed");

    dbox.p.x = fixed2float(clip_path->outer_box.p.x);
    dbox.p.y = fixed2float(clip_path->outer_box.p.y);
    dbox.q.x = fixed2float(clip_path->outer_box.q.x);
    dbox.q.y = fixed2float(clip_path->outer_box.q.y);
    gs_bbox_transform_inverse(&dbox, &ctm_only(ctx->pgs), ubox);
}

/* This will get the proper bounds based upon the current path, clip path
   and stroke width */
int xps_bounds_in_user_space_path_clip(xps_context_t *ctx, gs_rect *ubox,
                                       bool use_path, bool is_stroke)
{
    int code;
    gs_rect bbox;

    if (!use_path)
        code = gx_curr_bbox(ctx->pgs, &bbox, NO_PATH);
    else {
        if (is_stroke)
            code = gx_curr_bbox(ctx->pgs, &bbox, PATH_STROKE);
        else
            code = gx_curr_bbox(ctx->pgs, &bbox, PATH_FILL);
    }
    if (code < 0)
        return code;
    gs_bbox_transform_inverse(&bbox, &ctm_only(ctx->pgs), ubox);
    return code;
}

int
xps_begin_opacity(xps_context_t *ctx, char *base_uri, xps_resource_t *dict,
        char *opacity_att, xps_item_t *opacity_mask_tag, bool use_path,
        bool is_stroke)
{
    gs_transparency_group_params_t tgp;
    gs_transparency_mask_params_t tmp;
    gs_rect bbox;
    float opacity;
    int save;
    int code;

    if (!opacity_att && !opacity_mask_tag)
        return 0;

    opacity = 1.0;
    if (opacity_att)
        opacity = atof(opacity_att);
    gs_setblendmode(ctx->pgs, BLEND_MODE_Normal);
    gs_setopacityalpha(ctx->pgs, opacity);

    code = xps_bounds_in_user_space_path_clip(ctx, &bbox, use_path, is_stroke);
    if (code < 0)
        return code;

    if (opacity_mask_tag)
    {
        gs_trans_mask_params_init(&tmp, TRANSPARENCY_MASK_Luminosity);
        gs_gsave(ctx->pgs);
        gs_setcolorspace(ctx->pgs, ctx->gray_lin);
        gs_begin_transparency_mask(ctx->pgs, &tmp, &bbox, 0);

        /* Create a path if we dont have one that defines the opacity.
           For example if we had a canvas opacity then we need to make
           the path for that opacity.  Otherwise we use the opacity path
           that was defined and its intesection with the clipping path. */
        if (!use_path)
        {
            gs_moveto(ctx->pgs, bbox.p.x, bbox.p.y);
            gs_lineto(ctx->pgs, bbox.p.x, bbox.q.y);
            gs_lineto(ctx->pgs, bbox.q.x, bbox.q.y);
            gs_lineto(ctx->pgs, bbox.q.x, bbox.p.y);
            gs_closepath(ctx->pgs);
        }

        /* opacity-only mode: use alpha value as gray color to create luminosity mask */
        save = ctx->opacity_only;
        ctx->opacity_only = 1;

        code = xps_parse_brush(ctx, base_uri, dict, opacity_mask_tag);
        if (code)
        {
            //gs_grestore(ctx->pgs);
            gs_end_transparency_mask(ctx->pgs, TRANSPARENCY_CHANNEL_Opacity);
            ctx->opacity_only = save;
            return gs_rethrow(code, "cannot parse opacity mask brush");
        }

        gs_end_transparency_mask(ctx->pgs, TRANSPARENCY_CHANNEL_Opacity);
        gs_grestore(ctx->pgs);
        gs_push_transparency_state(ctx->pgs);
        ctx->opacity_only = save;
    }

    gs_trans_group_params_init(&tgp);
    gs_begin_transparency_group(ctx->pgs, &tgp, &bbox, PDF14_BEGIN_TRANS_GROUP);

    return 0;
}

void
xps_end_opacity(xps_context_t *ctx, char *base_uri, xps_resource_t *dict,
        char *opacity_att, xps_item_t *opacity_mask_tag)
{
    if (!opacity_att && !opacity_mask_tag)
        return;
    gs_end_transparency_group(ctx->pgs);
    /* Need to remove the soft mask from the graphic state.  Otherwise
       we may end up using it in subsequent drawings.  Note that there
       is not a push of the state made since there is already a soft
       mask present from gs_end_transparency_mask.  In this case,
       we are removing the mask with this forced pop. */
    if (opacity_mask_tag != NULL)
        gs_pop_transparency_state(ctx->pgs, true);
}
