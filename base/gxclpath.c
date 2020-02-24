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


/* Higher-level path operations for band lists */
#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gsptype2.h"
#include "gsptype1.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gxcolor2.h"
#include "gxdcolor.h"
#include "gxpcolor.h"
#include "gxpaint.h"		/* for gx_fill/stroke_params */
#include "gzpath.h"
#include "gzcpath.h"
#include "stream.h"
#include "gsserial.h"
#include "gxdevsop.h"

/* Statistics */
#if defined(DEBUG) && !defined(GS_THREADSAFE)
ulong stats_cmd_diffs[5];
#endif

/* Forward declarations */
static int cmd_put_path(gx_device_clist_writer * cldev,
                         gx_clist_state * pcls, const gx_path * ppath,
                         fixed ymin, fixed ymax, byte op,
                         bool implicit_close, segment_notes keep_notes);

/* ------ Utilities ------ */

/* Compute the colors used by a colored halftone. */
static gx_color_index
colored_halftone_color_usage(gx_device_clist_writer *cldev,
                             const gx_drawing_color *pdcolor)
{
    /*
     * We only know how to compute an accurate color set for the
     * standard CMYK color mapping function.
     */
    if (dev_proc(cldev, dev_spec_op)((gx_device *)cldev,
                                     gxdso_is_std_cmyk_1bit, NULL, 0) <= 0)
        return ((gx_color_index)1 << cldev->color_info.depth) - 1;  /* What about transparency?  Need to check this */
    /*
     * Note that c_base[0], and the low-order bit of plane_mask,
     * correspond to cyan: this requires reversing the bit order of
     * the plane mask.
     */
    return
        ((pdcolor->colors.colored.c_base[0] << 3) |
         (pdcolor->colors.colored.c_base[1] << 2) |
         (pdcolor->colors.colored.c_base[2] << 1) |
         (pdcolor->colors.colored.c_base[3]) |
         (byte_reverse_bits[pdcolor->colors.colored.plane_mask] >> 4));
}

/*
 * Compute whether a drawing operation will require the slow (full-pixel)
 * RasterOp implementation.  If pdcolor is not NULL, it is the texture for
 * the RasterOp.
 */
bool
cmd_slow_rop(gx_device *dev, gs_logical_operation_t lop,
    const gx_drawing_color *pdcolor)
{
    gs_rop3_t rop = lop_rop(lop);

    if (pdcolor != 0 && gx_dc_is_pure(pdcolor)) {
        gx_color_index color = gx_dc_pure_color(pdcolor);

        if (color == gx_device_black(dev))
            rop = rop3_know_T_0(rop);
        else if (color == gx_device_white(dev))
            rop = rop3_know_T_1(rop);
    }
    return !(rop == rop3_0 || rop == rop3_1 ||
             rop == rop3_S || rop == rop3_T);
}

/* Write out the color for filling, stroking, or masking. */
/* We should be able to share this with clist_tile_rectangle, */
/* but I don't see how to do it without adding a level of procedure. */
/* If the pattern color is big, it can write to "all" bands. */
int
cmd_put_drawing_color(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                      const gx_drawing_color * pdcolor, cmd_rects_enum_t *pre,
                      dc_devn_cl_type devn_type)
{
    const gx_device_halftone * pdht = pdcolor->type->get_dev_halftone(pdcolor);
    int                        code, di;
    uint                       dc_size = 0, req_size;
    gx_device_color_saved *    psdc = &pcls->sdc;
    byte *                     dp;
    byte *                     dp0;
    gs_int_point               color_phase;
    int			       buffer_space;
    int			       offset = 0;
    int			       left;
    uint		       portion_size, prefix_size;
    int			       req_size_final;
    bool		       is_pattern;
    gs_id		       pattern_id = gs_no_id;
    bool		       all_bands = (pre == NULL);

    /* see if the halftone must be inserted in the command list */
    if ( pdht != NULL                          &&
         pdht->id != cldev->device_halftone_id   ) {
        if ((code = cmd_put_halftone(cldev, pdht)) < 0)
            return code;
        psdc->type = gx_dc_type_none;	/* force writing */
    }

    if (psdc->devn_type != devn_type) {
        psdc->type = gx_dc_type_none;	/* force writing if fill/stroke mismatch. */
        psdc->devn_type = devn_type;
    }
    /*
     * Get the device color type index and the required size.
     *
     * The complete cmd_opv_ext_put_drawing_color consists of:
     *  command code (2 bytes)
     *  tile index value or non tile color (1)
     *  device color type index (1)
     *  length of serialized device color (enc_u_sizew(dc_size))
     *  the serialized device color itself (dc_size)
     */
    di = gx_get_dc_type_index(pdcolor);
    code = pdcolor->type->write( pdcolor,
                                 psdc,
                                 (gx_device *)cldev,
                                 0,
                                 0,
                                 &dc_size );

    /* if the returned value is > 0, no change in the color is necessary */
    if (code > 0 && ((devn_type == devn_not_tile_fill) || (devn_type == devn_not_tile_stroke)))
        return 0;
    else if (code < 0 && code != gs_error_rangecheck)
        return code;
    if (!all_bands && dc_size * pre->rect_nbands > 1024*1024 /* arbitrary */)
        all_bands = true;
    is_pattern = gx_dc_is_pattern1_color(pdcolor);
    if (is_pattern)
        pattern_id = gs_dc_get_pattern_id(pdcolor);
    if (all_bands) {
        gx_clist_state * pcls1;

        for (pcls1 = cldev->states; pcls1 < cldev->states + cldev->nbands; pcls1++) {
            if (pcls1->pattern_id == pattern_id) {
                pcls->pattern_id = gs_no_id; /* Force writing entire pattern. */
                break;
            }
        }
    }
    left = dc_size;

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);
    /* see if phase informaiton must be inserted in the command list */
    if ( pdcolor->type->get_phase(pdcolor, &color_phase) &&
         (color_phase.x != pcls->tile_phase.x ||
          color_phase.y != pcls->tile_phase.y || all_bands)        &&
         (code = cmd_set_tile_phase_generic( cldev,
                                     pcls,
                                     color_phase.x,
                                     color_phase.y, all_bands)) < 0  )
        return code;

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);
    if (is_pattern) {
        pattern_id = gs_dc_get_pattern_id(pdcolor);

        if (pattern_id != gs_no_id && pcls->pattern_id == pattern_id) {
            /* The pattern is known, write its id only.
               Note that gx_dc_pattern_write must process this case especially. */
            /* Note that id is gs_no_id when the pattern supplies an empty tile.
               In this case the full serialized pattern is shorter (left == 0),
               so go with it. */
            left = sizeof(pattern_id);
        }
    }

    do {
        prefix_size = 2 + 1 + (offset > 0 ? enc_u_sizew(offset) : 0);
        req_size = left + prefix_size + enc_u_sizew(left);
        CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);
        code = cmd_get_buffer_space(cldev, pcls, req_size);
        if (code < 0)
            return code;
        buffer_space = min(code, req_size);
        portion_size = buffer_space - prefix_size - enc_u_sizew(left);
        req_size_final = portion_size + prefix_size + enc_u_sizew(portion_size);
        if (req_size_final > buffer_space)
            return_error(gs_error_unregistered); /* Must not happen. */
        CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);
        if (all_bands)
            code = set_cmd_put_all_op(&dp, cldev, cmd_opv_extend, req_size_final);
        else
            code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_extend, req_size_final);
        if (code < 0)
            return code;
        dp0 = dp;
        switch (devn_type) {
            case devn_not_tile_fill:
                dp[1] = cmd_opv_ext_put_fill_dcolor;
                break;
            case devn_not_tile_stroke:
                dp[1] = cmd_opv_ext_put_stroke_dcolor;
                break;
            case devn_tile0:
                dp[1] = cmd_opv_ext_put_tile_devn_color0;
                break;
            case devn_tile1:
                dp[1] = cmd_opv_ext_put_tile_devn_color1;
                break;
            default:
                dp[1] = cmd_opv_ext_put_fill_dcolor;
        }
        dp += 2;
        *dp++ = di | (offset > 0 ? 0x80 : 0);
        if (offset > 0)
            enc_u_putw(offset, dp);
        enc_u_putw(portion_size, dp);
        code = pdcolor->type->write( pdcolor,
                                     &pcls->sdc,
                                     (gx_device *)cldev,
                                     offset,
                                     dp,
                                     &portion_size);
        CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev);
        if (code < 0) {
            if (offset == 0)
                cldev->cnext = dp0;
            return code;
        }
        offset += portion_size;
        left -= portion_size;
    } while (left);

    /* attempt to properly calculate color_usage */
    pcls->color_usage.or |= cmd_drawing_color_usage(cldev, pdcolor);

    /* record the color we have just serialized color */
    pdcolor->type->save_dc(pdcolor, &pcls->sdc);
    if (pattern_id != gs_no_id) {
        /* Don't record empty tiles because they're not cached. */
        pcls->pattern_id = pattern_id;
    }
    if (is_pattern) {
        /* HACK: since gx_dc_pattern_write identifies pattern by tile id,
           replace the client's pattern id with tile id in the saved color.  */
        pcls->sdc.colors.pattern.id = pattern_id;
        if (pattern_id &&
            (gx_pattern1_get_transptr(pdcolor) != NULL ||
             gx_pattern1_clist_has_trans(pdcolor))) {
            /* update either this band or all bands with the trans_bbox */
            if (all_bands) {
                pcls->color_usage.trans_bbox.p.x = 0;
                pcls->color_usage.trans_bbox.q.x = cldev->width;  /* no other information available */
                pcls->color_usage.trans_bbox.p.y = 0;
                pcls->color_usage.trans_bbox.q.y = cldev->height;
                clist_update_trans_bbox(cldev, &(pcls->color_usage.trans_bbox));
            } else {
                pcls->color_usage.trans_bbox.p.x = 0;
                pcls->color_usage.trans_bbox.q.x = cldev->width;  /* no other information available */
                pcls->color_usage.trans_bbox.p.y = pre->y;
                pcls->color_usage.trans_bbox.q.y = pre->yend;
            }
        }
    }
    if (is_pattern && all_bands) {
        /* Distribute the written pattern params to all bands.
           We know it is big, so it is not empty, so it has pattern_id and tile_phase.
         */
        gx_clist_state * pcls1;

        for (pcls1 = cldev->states; pcls1 < cldev->states + cldev->nbands; pcls1++) {
            pcls1->sdc = pcls->sdc;
            pcls1->pattern_id = pcls->pattern_id;
            pcls1->tile_phase.x = pcls->tile_phase.x;
            pcls1->tile_phase.y = pcls->tile_phase.y;
            pcls1->color_usage.or = pcls->color_usage.or;
        }
    }
    return code;
}

/* Compute the colors used by a drawing color. */
/* If the device is using transparency, the pdf14 compositor may have */
/* altered the colorspace. If so, just flag all components used.      */
gx_color_usage_bits
cmd_drawing_color_usage(gx_device_clist_writer *cldev,
                        const gx_drawing_color * pdcolor)
{
    if (cldev->page_uses_transparency &&
        (cldev->color_info.polarity != cldev->clist_color_info.polarity ||
        (cldev->color_info.num_components != cldev->clist_color_info.num_components))) {
        /* we would have to transform the color which would impact performance */
        return gx_color_usage_all(cldev);
    }

    if (gx_dc_is_pure(pdcolor))
        return gx_color_index2usage((gx_device *)cldev, gx_dc_pure_color(pdcolor));
    else if (gx_dc_is_binary_halftone(pdcolor))
        return gx_color_index2usage((gx_device *)cldev,
                                    gx_color_index2usage((gx_device *)cldev, gx_dc_binary_color0(pdcolor)) |
                                    gx_color_index2usage((gx_device *)cldev, gx_dc_binary_color1(pdcolor)));
    else if (gx_dc_is_colored_halftone(pdcolor))
        return gx_color_index2usage((gx_device *)cldev, colored_halftone_color_usage(cldev, pdcolor));
    else if (gx_dc_is_devn(pdcolor)) {
        gx_color_usage_bits bits = 0;

        gx_dc_devn_get_nonzero_comps(pdcolor, (gx_device *)cldev, &bits);
        return bits;
    }
    else
        return gx_color_usage_all(cldev);
}

/* Clear (a) specific 'known' flag(s) for all bands. */
/* We must do this whenever the value of a 'known' parameter changes. */
void
cmd_clear_known(gx_device_clist_writer * cldev, uint known)
{
    uint unknown = ~known;
    gx_clist_state *pcls = cldev->states;
    int i;

    for (i = cldev->nbands; --i >= 0; ++pcls)
        pcls->known &= unknown;
}

/* Check whether we need to change the clipping path in the device. */
bool
cmd_check_clip_path(gx_device_clist_writer * cldev, const gx_clip_path * pcpath)
{
    if (pcpath == NULL)
        return false;
    /* The clip path might have moved in memory, so even if the */
    /* ids match, update the pointer. */
    cldev->clip_path = pcpath;
    if (pcpath->id == cldev->clip_path_id)
        return false;
    cldev->clip_path_id = pcpath->id;
    return true;
}

/*
 * Check the graphics state elements that need to be up to date for filling
 * or stroking.
 */
#define FILL_KNOWN\
 (cj_ac_sa_known | flatness_known | op_bm_tk_known | opacity_alpha_known |\
  shape_alpha_known | fill_adjust_known | alpha_known | clip_path_known)
static void
cmd_check_fill_known(gx_device_clist_writer* cdev, const gs_gstate* pgs,
    double flatness, const gs_fixed_point* padjust,
    const gx_clip_path* pcpath, uint* punknown)
{
    /*
     * stroke_adjust is not needed for fills, and none of these are needed
     * if the path has no curves, but it's easier to update them all.
     */
    if (state_neq(line_params.curve_join) || state_neq(accurate_curves) ||
        state_neq(stroke_adjust)
        ) {
        *punknown |= cj_ac_sa_known;
        state_update(line_params.curve_join);
        state_update(accurate_curves);
        state_update(stroke_adjust);
    }
    if (cdev->gs_gstate.flatness != flatness) {
        *punknown |= flatness_known;
        cdev->gs_gstate.flatness = flatness;
    }
    /*
     * Note: overprint and overprint_mode are implemented via a compositor
     * device, which is passed separately through the command list. Hence,
     * though both parameters are passed in the state as well, this usually
     * has no effect.
     */
    if (state_neq(overprint) || state_neq(overprint_mode) ||
        state_neq(blend_mode) || state_neq(text_knockout) ||
        state_neq(stroke_overprint) || state_neq(renderingintent)) {
        *punknown |= op_bm_tk_known;
        state_update(overprint);
        state_update(overprint_mode);
        state_update(blend_mode);
        state_update(text_knockout);
        state_update(stroke_overprint);
        state_update(renderingintent);
    }
    if (state_neq(opacity.alpha)) {
        *punknown |= opacity_alpha_known;
        state_update(opacity.alpha);
    }
    if (state_neq(shape.alpha)) {
        *punknown |= shape_alpha_known;
        state_update(shape.alpha);
    }
    if (cdev->gs_gstate.fill_adjust.x != padjust->x ||
        cdev->gs_gstate.fill_adjust.y != padjust->y
        ) {
        *punknown |= fill_adjust_known;
        cdev->gs_gstate.fill_adjust = *padjust;
    }
    if (cdev->gs_gstate.alpha != pgs->alpha) {
        *punknown |= alpha_known;
        state_update(alpha);
    }
    if (cmd_check_clip_path(cdev, pcpath))
        *punknown |= clip_path_known;
}

/* Compute the written CTM length. */
int
cmd_write_ctm_return_length(gx_device_clist_writer * cldev, const gs_matrix *m)
{
    stream s;

    s_init(&s, cldev->memory);
    swrite_position_only(&s);
    sput_matrix(&s, m);
    return (uint)stell(&s);
}

/* Compute the written CTM length. */
int
cmd_write_ctm_return_length_nodevice(const gs_matrix *m)
{
    stream s;

    s_init(&s, NULL);
    swrite_position_only(&s);
    sput_matrix(&s, m);
    return (uint)stell(&s);
}

/* Write out CTM. */
int
cmd_write_ctm(const gs_matrix *m, byte *dp, int len)
{
    stream s;

    s_init(&s, NULL);
    swrite_string(&s, dp + 1, len);
    sput_matrix(&s, m);
    return 0;
}

/* Write out values of any unknown parameters. */
int
cmd_write_unknown(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                  uint must_know)
{
    uint unknown = ~pcls->known & must_know;
    uint misc2_unknown = unknown & misc2_all_known;
    byte *dp;
    int code;

    if (misc2_unknown) {
        byte buf[
                 1 +		/* cap_join: start_cap|join */
                 1 +            /*           end_cap|dash_cap */
                 1 +		/* cj_ac_sa */
                 sizeof(float) +	/* flatness */
                 sizeof(float) +	/* line width */
                 sizeof(float) +	/* miter limit */
                 3 +		/* bm_tk, op, and rend intent */
                 sizeof(float) * 2 +  /* opacity/shape alpha */
                 sizeof(cldev->gs_gstate.alpha)
        ];
        byte *bp = buf;
        if (unknown & cap_join_known) {
            *bp++ = (cldev->gs_gstate.line_params.start_cap << 3) +
                cldev->gs_gstate.line_params.join;
            *bp++ = (cldev->gs_gstate.line_params.end_cap << 3) +
                cldev->gs_gstate.line_params.dash_cap;
        }
        if (unknown & cj_ac_sa_known) {
            *bp++ =
                ((cldev->gs_gstate.line_params.curve_join + 1) << 2) +
                (cldev->gs_gstate.accurate_curves ? 2 : 0) +
                (cldev->gs_gstate.stroke_adjust ? 1 : 0);
        }
        if (unknown & flatness_known) {
            memcpy(bp, &cldev->gs_gstate.flatness, sizeof(float));
            bp += sizeof(float);
        }
        if (unknown & line_width_known) {
            float width =
                gx_current_line_width(&cldev->gs_gstate.line_params);

            memcpy(bp, &width, sizeof(width));
            bp += sizeof(width);
        }
        if (unknown & miter_limit_known) {
            memcpy(bp, &cldev->gs_gstate.line_params.miter_limit,
                   sizeof(float));
            bp += sizeof(float);
        }
        if (unknown & op_bm_tk_known) {
            *bp++ =
                ((int)cldev->gs_gstate.blend_mode << 3) +
                cldev->gs_gstate.text_knockout;
            *bp++ =
                (cldev->gs_gstate.overprint_mode << 2) +
                (cldev->gs_gstate.stroke_overprint << 1) +
                cldev->gs_gstate.overprint;
            *bp++ = cldev->gs_gstate.renderingintent;
        }
        if (unknown & opacity_alpha_known) {
            memcpy(bp, &cldev->gs_gstate.opacity.alpha, sizeof(float));
            bp += sizeof(float);
        }
        if (unknown & shape_alpha_known) {
            memcpy(bp, &cldev->gs_gstate.shape.alpha, sizeof(float));
            bp += sizeof(float);
        }
        if (unknown & alpha_known) {
            memcpy(bp, &cldev->gs_gstate.alpha,
                   sizeof(cldev->gs_gstate.alpha));
            bp += sizeof(cldev->gs_gstate.alpha);
        }
        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_misc2,
                              1 + cmd_sizew(misc2_unknown) + (bp - buf));
        if (code < 0)
            return 0;
        memcpy(cmd_put_w(misc2_unknown, dp + 1), buf, bp - buf);
        pcls->known |= misc2_unknown;
    }
    if (unknown & fill_adjust_known) {
        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_fill_adjust,
                              1 + sizeof(fixed) * 2);
        if (code < 0)
            return code;
        memcpy(dp + 1, &cldev->gs_gstate.fill_adjust.x, sizeof(fixed));
        memcpy(dp + 1 + sizeof(fixed), &cldev->gs_gstate.fill_adjust.y, sizeof(fixed));
        pcls->known |= fill_adjust_known;
    }
    if (unknown & ctm_known) {
        int len = cmd_write_ctm_return_length(cldev, &ctm_only(&cldev->gs_gstate));

        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_ctm, len + 1);
        if (code < 0)
            return code;
        code = cmd_write_ctm(&ctm_only(&cldev->gs_gstate), dp, len);
        if (code < 0)
            return code;
        pcls->known |= ctm_known;
    }
    if (unknown & dash_known) {
        int n = cldev->gs_gstate.line_params.dash.pattern_size;

        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_dash,
                              2 + (n + 2) * sizeof(float));
        if (code < 0)
            return code;
        dp[1] = n + (cldev->gs_gstate.line_params.dash.adapt ? 0x80 : 0) +
            (cldev->gs_gstate.line_params.dot_length_absolute ? 0x40 : 0);
        memcpy(dp + 2, &cldev->gs_gstate.line_params.dot_length,
               sizeof(float));
        memcpy(dp + 2 + sizeof(float),
               &cldev->gs_gstate.line_params.dash.offset,
               sizeof(float));
        if (n != 0)
            memcpy(dp + 2 + sizeof(float) * 2,
                   cldev->dash_pattern, n * sizeof(float));
        pcls->known |= dash_known;
    }
    if (unknown & clip_path_known) {
        /*
         * We can write out the clipping path either as rectangles
         * or as a real (filled) path.
         */
        const gx_clip_path *pcpath = cldev->clip_path;
        int band_height = cldev->page_band_height;
        int ymin = (pcls - cldev->states) * band_height;
        int ymax = min(ymin + band_height, cldev->height);
        gs_fixed_rect box;
        bool punt_to_outer_box = false;
        int code;

        code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_begin_clip, 1);
        if (code < 0)
            return code;
        if (pcpath->path_valid) {
            if (gx_path_is_rectangle(&pcpath->path, &box) &&
                fixed_is_int(box.p.x | box.p.y | box.q.x | box.q.y)
                ) {
                /* Write the path as a rectangle. */
                code = cmd_write_rect_cmd(cldev, pcls, cmd_op_fill_rect,
                                          fixed2int_var(box.p.x),
                                          fixed2int_var(box.p.y),
                                          fixed2int(box.q.x - box.p.x),
                                          fixed2int(box.q.y - box.p.y));
            } else if ( !(cldev->disable_mask & clist_disable_complex_clip) ) {
                /* Write the path. */
                code = cmd_put_path(cldev, pcls, &pcpath->path,
                                    int2fixed(ymin - 1),
                                    int2fixed(ymax + 1),
                                    (byte)(pcpath->rule == gx_rule_even_odd ?
                                     cmd_opv_eofill : cmd_opv_fill),
                                    true, sn_not_first);
            } else {
                  /* Complex paths disabled: write outer box as clip */
                  punt_to_outer_box = true;
            }
        } else {		/* Write out the rectangles. */
            const gx_clip_list *list = gx_cpath_list(pcpath);
            const gx_clip_rect *prect = list->head;

            if (prect == 0)
                prect = &list->single;
            else if (cldev->disable_mask & clist_disable_complex_clip)
                punt_to_outer_box = true;
            if (!punt_to_outer_box) {
                for (; prect != 0 && code >= 0; prect = prect->next)
                    if (prect->xmax > prect->xmin &&
                        prect->ymin < ymax && prect->ymax > ymin
                        ) {
                        code =
                            cmd_write_rect_cmd(cldev, pcls, cmd_op_fill_rect,
                                               prect->xmin, prect->ymin,
                                               prect->xmax - prect->xmin,
                                       prect->ymax - prect->ymin);
                    }
            }
        }
        if (punt_to_outer_box) {
            /* Clip is complex, but disabled. Write out the outer box */
            gs_fixed_rect box;

            gx_cpath_outer_box(pcpath, &box);
            box.p.x = fixed_floor(box.p.x);
            box.p.y = fixed_floor(box.p.y);
            code = cmd_write_rect_cmd(cldev, pcls, cmd_op_fill_rect,
                                      fixed2int_var(box.p.x),
                                      fixed2int_var(box.p.y),
                                      fixed2int_ceiling(box.q.x - box.p.x),
                                      fixed2int_ceiling(box.q.y - box.p.y));
        }
        {
            int end_code =
                set_cmd_put_op(&dp, cldev, pcls, cmd_opv_end_clip, 1);

            if (code >= 0)
                code = end_code;	/* take the first failure seen */
            if (end_code < 0) {
                /*
                 * end_clip has to work despite lo-mem to maintain consistency.
                 * This isn't error recovery, but just to prevent dangling
                 * cmd_opv_begin_clip's.
                 */
                ++cldev->ignore_lo_mem_warnings;
                end_code =
                    set_cmd_put_op(&dp, cldev, pcls, cmd_opv_end_clip, 1);
                --cldev->ignore_lo_mem_warnings;
            }
        }
        if (code < 0)
            return code;
        pcls->clip_enabled = 1;
        pcls->known |= clip_path_known;
    }
    if (unknown & color_space_known) {
        byte *dp;

        if (cldev->color_space.byte1 & 8) {	/* indexed */
            const gs_color_space *pcs = cldev->color_space.space;
            int hival = pcs->params.indexed.hival;
            uint num_values = (hival + 1) *
                gs_color_space_num_components(pcs->base_space);
            bool use_proc = cldev->color_space.byte1 & 4;
            const void *map_data;
            uint map_size;

            if (use_proc) {
                map_data = pcs->params.indexed.lookup.map->values;
                map_size = num_values *
                    sizeof(pcs->params.indexed.lookup.map->values[0]);
            } else {
                map_data = pcs->params.indexed.lookup.table.data;
                map_size = num_values;
            }
            code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_color_space,
                                  2 + cmd_sizew(hival) + map_size +
                                  sizeof(clist_icc_color_t));
            if (code < 0)
                return code;
            /* Save the ICC information */
            memcpy(dp + 2, &(cldev->color_space.icc_info),
                   sizeof(clist_icc_color_t));
            memcpy(cmd_put_w(hival, dp + 2 +
                   sizeof(clist_icc_color_t)), map_data, map_size);
        } else {
            code = set_cmd_put_op(&dp, cldev, pcls, cmd_opv_set_color_space,
                2 + sizeof(clist_icc_color_t));
            memcpy(dp + 2, &(cldev->color_space.icc_info),
                   sizeof(clist_icc_color_t));
            if (code < 0)
                return code;
        }
        dp[1] = cldev->color_space.byte1;
        pcls->known |= color_space_known;
    }
    /****** HANDLE masks ******/
    return 0;
}

/* ------ Driver procedures ------ */

int
clist_fill_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
            const gx_fill_params * params, const gx_drawing_color * pdcolor,
                const gx_clip_path * pcpath)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    uint unknown = 0;
    int ry, rheight, rx, rwidth, y0, y1;
    gs_logical_operation_t lop = pgs->log_op;
    byte op = (byte)
        (params->rule == gx_rule_even_odd ?
         cmd_opv_eofill : cmd_opv_fill);
    gs_fixed_point adjust;
    bool slow_rop = cmd_slow_rop(dev, lop_know_S_0(lop), pdcolor);
    cmd_rects_enum_t re;
    int code;

    adjust = params->adjust;
    {
        gs_fixed_rect bbox;

        if (ppath != NULL)
            gx_path_bbox(ppath, &bbox);
        else {
            /* gx_default_fill_path passes the clip path for shfill. */
            gx_cpath_outer_box(pcpath, &bbox);
        }
        ry = fixed2int(bbox.p.y) - 1;
        rheight = fixed2int_ceiling(bbox.q.y) - ry + 1;
        crop_fill_y(cdev, ry, rheight);
        if (rheight <= 0)
            return 0;
        rx = fixed2int(bbox.p.x) - 1;
        rwidth = fixed2int_ceiling(bbox.q.x) - rx + 1;
        fit_fill_w(cdev, rx, rwidth);
    }
    if ( (cdev->disable_mask & clist_disable_fill_path) ||
         gs_debug_c(',')
         ) {
        /* Disable path-based banding. */
        return gx_default_fill_path(dev, pgs, ppath, params, pdcolor,
                                    pcpath);
    }
    if (pdcolor != NULL && gx_dc_is_pattern2_color(pdcolor)) {
        /* Here we need to intersect *ppath, *pcpath and shading bbox.
           Call the default implementation, which has a special
           branch for processing a shading fill with the clip writer device.
           It will call us back with pdcolor=NULL for passing
           the intersected clipping path,
           and then will decompose the shading into trapezoids.
           See comment below about pdcolor == NULL.
         */
        cdev->cropping_saved = false;
        code = gx_default_fill_path(dev, pgs, ppath, params, pdcolor, pcpath);
        if (cdev->cropping_saved) {
            cdev->cropping_min = cdev->save_cropping_min;
            cdev->cropping_max = cdev->save_cropping_max;
            if_debug2m('v', cdev->memory,
                       "[v] clist_fill_path: restore cropping_min=%d croping_max=%d\n",
                       cdev->save_cropping_min, cdev->save_cropping_max);
        }
        return code;
    }
    y0 = ry;
    y1 = ry + rheight;
    cmd_check_fill_known(cdev, pgs, params->flatness, &adjust, pcpath,
                         &unknown);
    if (unknown)
        cmd_clear_known(cdev, unknown);
    if (cdev->permanent_error < 0)
        return (cdev->permanent_error);
    if (pdcolor == NULL) {
        /* See comment above about pattern2_color.
           Put the clipping path only.
           The graphics library will call us again with subdividing
           the shading into trapezoids and rectangles.
           Narrow cropping_min, croping_max for such calls. */
        cdev->cropping_saved = true;
        cdev->save_cropping_min = cdev->cropping_min;
        cdev->save_cropping_max = cdev->cropping_max;
        cdev->cropping_min = max(ry, cdev->cropping_min);
        cdev->cropping_max = min(ry + rheight, cdev->cropping_max);
        if_debug2m('v', cdev->memory,
                   "[v] clist_fill_path: narrow cropping_min=%d croping_max=%d\n",
                   cdev->save_cropping_min, cdev->save_cropping_max);
        RECT_ENUM_INIT(re, ry, rheight);
        do {
            RECT_STEP_INIT(re);
            if (pcpath != NULL) {
                code = cmd_do_write_unknown(cdev, re.pcls, clip_path_known);
                if (code < 0)
                    return code;
            }
            code = cmd_do_enable_clip(cdev, re.pcls, pcpath != NULL);
            if (code  < 0)
                return code;
            re.y += re.height;
        } while (re.y < re.yend);
    } else {
        /* We should not reach here with ppath==NULL (pdcolor != NULL, so not a shading fill */
        if (ppath == NULL)
            return_error(gs_error_unregistered);

        /* If needed, update the trans_bbox */
        if (cdev->pdf14_needed) {
            gs_int_rect bbox;

            bbox.p.x = rx;
            bbox.q.x = rx + rwidth - 1;
            bbox.p.y = ry;
            bbox.q.y = ry + rheight - 1;

            clist_update_trans_bbox(cdev, &bbox);
        }

        RECT_ENUM_INIT(re, ry, rheight);
        do {
            RECT_STEP_INIT(re);
            code = cmd_do_write_unknown(cdev, re.pcls, FILL_KNOWN);
            if (code < 0)
                return code;
            if ((code = cmd_do_enable_clip(cdev, re.pcls, pcpath != NULL)) < 0 ||
                (code = cmd_update_lop(cdev, re.pcls, lop)) < 0
                )
                return code;
            code = cmd_put_drawing_color(cdev, re.pcls, pdcolor, &re,
                                         devn_not_tile_fill);
            if (code == gs_error_unregistered)
                return code;
            if (code < 0) {
                /* Something went wrong, use the default implementation. */
                return gx_default_fill_path(dev, pgs, ppath, params, pdcolor,
                                            pcpath);
            }
            re.pcls->color_usage.slow_rop |= slow_rop;
            code = cmd_put_path(cdev, re.pcls, ppath,
                                int2fixed(max(re.y - 1, y0)),
                                int2fixed(min(re.y + re.height + 1, y1)),
                                op,
                                true, sn_none /* fill doesn't need the notes */ );
            if (code < 0)
                return code;
            re.y += re.height;
        } while (re.y < re.yend);
    }
    return 0;
}

int
clist_fill_stroke_path(gx_device * pdev, const gs_gstate * pgs,
                            gx_path * ppath,
                            const gx_fill_params * params_fill,
                            const gx_device_color * pdevc_fill,
                            const gx_stroke_params * params_stroke,
                            const gx_device_color * pdevc_stroke,
                            const gx_clip_path * pcpath)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)pdev)->writer;
    int pattern_size = pgs->line_params.dash.pattern_size;
    byte op = (byte) (params_fill->rule == gx_rule_even_odd ?
                  cmd_opv_eofill_stroke : cmd_opv_fill_stroke);
    uint unknown = 0;
    gs_fixed_rect bbox;
    gs_fixed_point expansion;
    int adjust_y, expansion_code;
    int ry, rheight;
    gs_logical_operation_t lop = pgs->log_op;
    bool slow_rop = cmd_slow_rop(pdev, lop_know_S_0(lop), pdevc_fill);
    cmd_rects_enum_t re;

    if (pdevc_stroke == NULL || pdevc_fill == NULL)
        return_error(gs_error_unknownerror);	/* shouldn't happen */

    if ((cdev->disable_mask & (clist_disable_fill_path || clist_disable_stroke_path)) ||
        gs_debug_c(',')
        ) {
        /* Disable path-based banding. */
        return gx_default_fill_stroke_path(pdev, pgs, ppath, params_fill, pdevc_fill,
                                           params_stroke, pdevc_stroke, pcpath);
    }
    /* TODO: For now punt to default if we have shaded color (pattern2) */
    if (gx_dc_is_pattern2_color(pdevc_fill) || gx_dc_is_pattern2_color(pdevc_stroke)) {
        return gx_default_fill_stroke_path(pdev, pgs, ppath, params_fill, pdevc_fill,
                                           params_stroke, pdevc_stroke, pcpath);
    }
    gx_path_bbox(ppath, &bbox);
    /* We must use the supplied gs_gstate, not our saved one, */
    /* for computing the stroke expansion. */
    expansion_code = gx_stroke_path_expansion(pgs, ppath, &expansion);
    if (expansion_code < 0) {
        /* Expansion is too large: use the entire page. */
        adjust_y = 0;
        ry = 0;
        rheight = pdev->height;
    } else {
        adjust_y = fixed2int_ceiling(expansion.y) + 1;
        ry = fixed2int(bbox.p.y) - adjust_y;
        rheight = fixed2int_ceiling(bbox.q.y) - ry + adjust_y;
        fit_fill_y(pdev, ry, rheight);
        fit_fill_h(pdev, ry, rheight);
        if (rheight <= 0)
            return 0;
    }
    /* Check the dash pattern, since we bail out if */
    /* the pattern is too large. */
    if (cdev->gs_gstate.line_params.dash.pattern_size != pattern_size ||
        (pattern_size != 0 &&
         memcmp(cdev->dash_pattern, pgs->line_params.dash.pattern,
                pattern_size * sizeof(float))) ||
        cdev->gs_gstate.line_params.dash.offset !=
          pgs->line_params.dash.offset ||
        cdev->gs_gstate.line_params.dash.adapt !=
          pgs->line_params.dash.adapt ||
        cdev->gs_gstate.line_params.dot_length !=
          pgs->line_params.dot_length ||
        cdev->gs_gstate.line_params.dot_length_absolute !=
          pgs->line_params.dot_length_absolute
    ) {
        /* Bail out if the dash pattern is too long. */
        if (pattern_size > cmd_max_dash)
            return gx_default_fill_stroke_path(pdev, pgs, ppath, params_fill, pdevc_fill,
                                               params_stroke, pdevc_stroke, pcpath);
        unknown |= dash_known;
        /*
         * Temporarily reset the dash pattern pointer for gx_set_dash,
         * but don't leave it set, since that would confuse the GC.
         */
        cdev->gs_gstate.line_params.dash.pattern = cdev->dash_pattern;
        gx_set_dash(&cdev->gs_gstate.line_params.dash,
                    pgs->line_params.dash.pattern,
                    pgs->line_params.dash.pattern_size,
                    pgs->line_params.dash.offset, NULL);
        cdev->gs_gstate.line_params.dash.pattern = 0;
        gx_set_dash_adapt(&cdev->gs_gstate.line_params.dash,
                          pgs->line_params.dash.adapt);
        gx_set_dot_length(&cdev->gs_gstate.line_params,
                          pgs->line_params.dot_length,
                          pgs->line_params.dot_length_absolute);
    }

    if (state_neq(line_params.start_cap) || state_neq(line_params.join) ||
        state_neq(line_params.end_cap) || state_neq(line_params.dash_cap)) {
        unknown |= cap_join_known;
        state_update(line_params.start_cap);
        state_update(line_params.end_cap);
        state_update(line_params.dash_cap);
        state_update(line_params.join);
    }
    cmd_check_fill_known(cdev, pgs, params_fill->flatness, &pgs->fill_adjust,
                         pcpath, &unknown);
    if (state_neq(line_params.half_width)) {
        unknown |= line_width_known;
        state_update(line_params.half_width);
    }
    if (state_neq(line_params.miter_limit)) {
        unknown |= miter_limit_known;
        gx_set_miter_limit(&cdev->gs_gstate.line_params,
                           pgs->line_params.miter_limit);
    }
    if (state_neq(ctm.xx) || state_neq(ctm.xy) ||
        state_neq(ctm.yx) || state_neq(ctm.yy) ||
    /* We don't actually need tx or ty, but we don't want to bother */
    /* tracking them separately from the other coefficients. */
        state_neq(ctm.tx) || state_neq(ctm.ty)
        ) {
        unknown |= ctm_known;
        state_update(ctm);
    }
    if (unknown)
        cmd_clear_known(cdev, unknown);
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    /* If needed, update the trans_bbox */
    if (cdev->pdf14_needed) {
        gs_int_rect trans_bbox;
        int rx = fixed2int(bbox.p.x) - 1;
        int rwidth = fixed2int_ceiling(bbox.q.x) - rx + 1;
        unknown |= STROKE_ALL_KNOWN;

        fit_fill_w(cdev, rx, rwidth);
        trans_bbox.p.x = rx;
        trans_bbox.q.x = rx + rwidth - 1;
        trans_bbox.p.y = ry;
        trans_bbox.q.y = ry + rheight - 1;

        clist_update_trans_bbox(cdev, &trans_bbox);
    }
    /* If either fill or stroke uses overprint, or overprint_mode != 0, then we */
    /* need to write out the overprint drawn_comps and retain_*			*/
    if (((pgs->overprint_mode || pgs->overprint || pgs->stroke_overprint))) {
        unknown |= op_bm_tk_known;
    }
    RECT_ENUM_INIT(re, ry, rheight);
    do {
        int code;

        RECT_STEP_INIT(re);
        if ((code = cmd_do_write_unknown(cdev, re.pcls, STROKE_ALL_KNOWN | FILL_KNOWN)) < 0)
            return code;
        if ((code = cmd_do_enable_clip(cdev, re.pcls, pcpath != NULL)) < 0)
            return code;
        if ((code = cmd_update_lop(cdev, re.pcls, lop)) < 0)
            return code;
        /* Write the stroke first since do_fill_stroke will have locked the pattern	*/
        /* tile if needed, and we want it locked after reading the stroke color.	*/
        code = cmd_put_drawing_color(cdev, re.pcls, pdevc_stroke, &re, devn_not_tile_stroke);
        if (code < 0) {
            /* Something went wrong, use the default implementation. */
            return gx_default_fill_stroke_path(pdev, pgs, ppath, params_fill, pdevc_fill,
                params_stroke, pdevc_stroke, pcpath);
        }
        code = cmd_put_drawing_color(cdev, re.pcls, pdevc_fill, &re, devn_not_tile_fill);
        if (code < 0) {
            /* Something went wrong, use the default implementation. */
            return gx_default_fill_stroke_path(pdev, pgs, ppath, params_fill, pdevc_fill,
                                               params_stroke, pdevc_stroke, pcpath);
        }
        re.pcls->color_usage.slow_rop |= slow_rop;

        /* Don't skip segments when expansion is unknown.  */

        code = cmd_put_path(cdev, re.pcls, ppath, min_fixed, max_fixed,
                            op, false, (segment_notes)~0);
        if (code < 0)
            return code;
        re.y += re.height;
    } while (re.y < re.yend);
    return 0;
}

int
clist_stroke_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
                  const gx_stroke_params * params,
              const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    int pattern_size = pgs->line_params.dash.pattern_size;
    uint unknown = 0;
    gs_fixed_rect bbox;
    gs_fixed_point expansion;
    int adjust_y, expansion_code;
    int ry, rheight;
    gs_logical_operation_t lop = pgs->log_op;
    bool slow_rop = cmd_slow_rop(dev, lop_know_S_0(lop), pdcolor);
    cmd_rects_enum_t re;

    CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);
    if ((cdev->disable_mask & clist_disable_stroke_path) ||
        gs_debug_c(',')
        ) {
        /* Disable path-based banding. */
        return gx_default_stroke_path(dev, pgs, ppath, params, pdcolor,
                                      pcpath);
    }
    gx_path_bbox(ppath, &bbox);
    /* We must use the supplied gs_gstate, not our saved one, */
    /* for computing the stroke expansion. */
    expansion_code = gx_stroke_path_expansion(pgs, ppath, &expansion);
    if (expansion_code < 0) {
        /* Expansion is too large: use the entire page. */
        adjust_y = 0;
        ry = 0;
        rheight = dev->height;
    } else {
        adjust_y = fixed2int_ceiling(expansion.y) + 1;
        ry = fixed2int(bbox.p.y) - adjust_y;
        rheight = fixed2int_ceiling(bbox.q.y) - ry + adjust_y;
        fit_fill_y(dev, ry, rheight);
        fit_fill_h(dev, ry, rheight);
        if (rheight <= 0)
            return 0;
    }
    /* Check the dash pattern, since we bail out if */
    /* the pattern is too large. */
    if (cdev->gs_gstate.line_params.dash.pattern_size != pattern_size ||
        (pattern_size != 0 &&
         memcmp(cdev->dash_pattern, pgs->line_params.dash.pattern,
                pattern_size * sizeof(float))) ||
        cdev->gs_gstate.line_params.dash.offset !=
          pgs->line_params.dash.offset ||
        cdev->gs_gstate.line_params.dash.adapt !=
          pgs->line_params.dash.adapt ||
        cdev->gs_gstate.line_params.dot_length !=
          pgs->line_params.dot_length ||
        cdev->gs_gstate.line_params.dot_length_absolute !=
          pgs->line_params.dot_length_absolute
    ) {
        /* Bail out if the dash pattern is too long. */
        if (pattern_size > cmd_max_dash)
            return gx_default_stroke_path(dev, pgs, ppath, params,
                                          pdcolor, pcpath);
        unknown |= dash_known;
        /*
         * Temporarily reset the dash pattern pointer for gx_set_dash,
         * but don't leave it set, since that would confuse the GC.
         */
        cdev->gs_gstate.line_params.dash.pattern = cdev->dash_pattern;
        gx_set_dash(&cdev->gs_gstate.line_params.dash,
                    pgs->line_params.dash.pattern,
                    pgs->line_params.dash.pattern_size,
                    pgs->line_params.dash.offset, NULL);
        cdev->gs_gstate.line_params.dash.pattern = 0;
        gx_set_dash_adapt(&cdev->gs_gstate.line_params.dash,
                          pgs->line_params.dash.adapt);
        gx_set_dot_length(&cdev->gs_gstate.line_params,
                          pgs->line_params.dot_length,
                          pgs->line_params.dot_length_absolute);
    }

    if (state_neq(line_params.start_cap) || state_neq(line_params.join) ||
        state_neq(line_params.end_cap) || state_neq(line_params.dash_cap)) {
        unknown |= cap_join_known;
        state_update(line_params.start_cap);
        state_update(line_params.end_cap);
        state_update(line_params.dash_cap);
        state_update(line_params.join);
    }
    cmd_check_fill_known(cdev, pgs, params->flatness, &pgs->fill_adjust,
                         pcpath, &unknown);
    if (state_neq(line_params.half_width)) {
        unknown |= line_width_known;
        state_update(line_params.half_width);
    }
    if (state_neq(line_params.miter_limit)) {
        unknown |= miter_limit_known;
        gx_set_miter_limit(&cdev->gs_gstate.line_params,
                           pgs->line_params.miter_limit);
    }
    if (state_neq(ctm.xx) || state_neq(ctm.xy) ||
        state_neq(ctm.yx) || state_neq(ctm.yy) ||
    /* We don't actually need tx or ty, but we don't want to bother */
    /* tracking them separately from the other coefficients. */
        state_neq(ctm.tx) || state_neq(ctm.ty)
        ) {
        unknown |= ctm_known;
        state_update(ctm);
    }
    if (unknown)
        cmd_clear_known(cdev, unknown);
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    /* If needed, update the trans_bbox */
    if (cdev->pdf14_needed) {
        gs_int_rect trans_bbox;
        int rx = fixed2int(bbox.p.x) - 1;
        int rwidth = fixed2int_ceiling(bbox.q.x) - rx + 1;

        fit_fill_w(cdev, rx, rwidth);
        trans_bbox.p.x = rx;
        trans_bbox.q.x = rx + rwidth - 1;
        trans_bbox.p.y = ry;
        trans_bbox.q.y = ry + rheight - 1;

        clist_update_trans_bbox(cdev, &trans_bbox);
    }
    RECT_ENUM_INIT(re, ry, rheight);
    do {
        int code;

        RECT_STEP_INIT(re);
        CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);
        if ((code = cmd_do_write_unknown(cdev, re.pcls, STROKE_ALL_KNOWN)) < 0 ||
            (code = cmd_do_enable_clip(cdev, re.pcls, pcpath != NULL)) < 0 ||
            (code = cmd_update_lop(cdev, re.pcls, lop)) < 0
            )
            return code;
        CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);
        code = cmd_put_drawing_color(cdev, re.pcls, pdcolor, &re, devn_not_tile_stroke);
            if (code == gs_error_unregistered)
                return code;
        if (code < 0) {
            /* Something went wrong, use the default implementation. */
            return gx_default_stroke_path(dev, pgs, ppath, params, pdcolor,
                                          pcpath);
        }
        re.pcls->color_usage.slow_rop |= slow_rop;
        CMD_CHECK_LAST_OP_BLOCK_DEFINED(cdev);
        {
            fixed ymin, ymax;

            /*
             * If a dash pattern is active, we can't skip segments
             * outside the clipping region, because that would throw off
             * the pattern.
             * Don't skip segments when expansion is unknown.
             */

            if (pattern_size || expansion_code < 0 ) {
                ymin = min_fixed;
                ymax = max_fixed;
            } else {
                ymin = int2fixed(re.y - adjust_y);
                ymax = int2fixed(re.y + re.height + adjust_y);
            }
            code = cmd_put_path(cdev, re.pcls, ppath, ymin, ymax,
                                cmd_opv_stroke,
                                false, (segment_notes)~0);
            if (code < 0)
                return code;
        }
        re.y += re.height;
    } while (re.y < re.yend);
    return 0;
}

/*
 * Fill_parallelogram and fill_triangle aren't very efficient.  This isn't
 * important right now, since the non-degenerate case is only used for
 * smooth shading.  However, the rectangular case of fill_parallelogram is
 * sometimes used for images, so its performance does matter.
 */

static int
clist_put_polyfill(gx_device *dev, fixed px, fixed py,
                   const gs_fixed_point *points, int num_points,
                   const gx_drawing_color *pdcolor, gs_logical_operation_t lop)
{
    gx_path path;
    gs_memory_t *mem = dev->memory;
    int code;
    gx_device_clist_writer * const cdev =
        &((gx_device_clist *)dev)->writer;
    gs_fixed_rect bbox;
    int ry, rheight, y0, y1;
    bool slow_rop = cmd_slow_rop(dev, lop_know_S_0(lop), pdcolor);
    cmd_rects_enum_t re;

    if (gs_debug_c(','))
        return -1;		/* path-based banding is disabled */
    gx_path_init_local(&path, mem);
    if ((code = gx_path_add_point(&path, px, py)) < 0 ||
        (code = gx_path_add_lines(&path, points, num_points)) < 0
        )
        goto out;
    gx_path_bbox(&path, &bbox);
    ry = fixed2int(bbox.p.y) - 1;
    rheight = fixed2int_ceiling(bbox.q.y) - ry + 1;
    fit_fill_y(dev, ry, rheight);
    fit_fill_h(dev, ry, rheight);
    if (rheight <= 0)
        return 0;
    y0 = ry;
    y1 = ry + rheight;
    if (cdev->permanent_error < 0)
      return (cdev->permanent_error);
    /* If needed, update the trans_bbox */
    if (cdev->pdf14_needed) {
        gs_int_rect trans_bbox;
        int rx = fixed2int(bbox.p.x) - 1;
        int rwidth = fixed2int_ceiling(bbox.q.x) - rx + 1;

        fit_fill_w(cdev, rx, rwidth);
        trans_bbox.p.x = rx;
        trans_bbox.q.x = rx + rwidth - 1;
        trans_bbox.p.y = ry;
        trans_bbox.q.y = ry + rheight - 1;

        clist_update_trans_bbox(cdev, &trans_bbox);
    }
    RECT_ENUM_INIT(re, ry, rheight);
    do {
        RECT_STEP_INIT(re);
        if ((code = cmd_update_lop(cdev, re.pcls, lop)) < 0 ||
            (code = cmd_put_drawing_color(cdev, re.pcls, pdcolor, &re, devn_not_tile_fill)) < 0)
            goto out;
        re.pcls->color_usage.slow_rop |= slow_rop;
        code = cmd_put_path(cdev, re.pcls, &path,
                            int2fixed(max(re.y - 1, y0)),
                            int2fixed(min(re.y + re.height + 1, y1)),
                            cmd_opv_polyfill,
                            true, sn_none /* fill doesn't need the notes */ );
        if (code < 0)
            goto out;
        re.y += re.height;
    } while (re.y < re.yend);
out:
    gx_path_free(&path, "clist_put_polyfill");
    return code;
}

int
clist_fill_parallelogram(gx_device *dev, fixed px, fixed py,
                         fixed ax, fixed ay, fixed bx, fixed by,
                         const gx_drawing_color *pdcolor,
                         gs_logical_operation_t lop)
{
    gs_fixed_point pts[3];
    int code;

    if (PARALLELOGRAM_IS_RECT(ax, ay, bx, by)) {
        gs_int_rect r;

        INT_RECT_FROM_PARALLELOGRAM(&r, px, py, ax, ay, bx, by);
        return gx_fill_rectangle_device_rop(r.p.x, r.p.y, r.q.x - r.p.x,
                                            r.q.y - r.p.y, pdcolor, dev, lop);
    }
    pts[0].x = px + ax, pts[0].y = py + ay;
    pts[1].x = pts[0].x + bx, pts[1].y = pts[0].y + by;
    pts[2].x = px + bx, pts[2].y = py + by;
    code = clist_put_polyfill(dev, px, py, pts, 3, pdcolor, lop);
    return (code >= 0 ? code :
            gx_default_fill_parallelogram(dev, px, py, ax, ay, bx, by,
                                          pdcolor, lop));
}

int
clist_fill_triangle(gx_device *dev, fixed px, fixed py,
                    fixed ax, fixed ay, fixed bx, fixed by,
                    const gx_drawing_color *pdcolor,
                    gs_logical_operation_t lop)
{
    gs_fixed_point pts[2];
    int code;

    pts[0].x = px + ax, pts[0].y = py + ay;
    pts[1].x = px + bx, pts[1].y = py + by;
    code = clist_put_polyfill(dev, px, py, pts, 2, pdcolor, lop);
    return (code >= 0 ? code :
            gx_default_fill_triangle(dev, px, py, ax, ay, bx, by,
                                     pdcolor, lop));
}

/* ------ Path utilities ------ */

/* Define the state bookkeeping for writing path segments. */
typedef struct cmd_segment_writer_s {
    /* Set at initialization */
    gx_device_clist_writer *cldev;
    gx_clist_state *pcls;
    /* Updated dynamically */
    segment_notes notes;
    byte *dp;
    int len;
    gs_fixed_point delta_first;
    byte cmd[6 * (1 + sizeof(fixed))];
}
cmd_segment_writer;

/* Put out a path segment command. */
static int
cmd_put_segment(cmd_segment_writer * psw, byte op,
                const fixed * operands, segment_notes notes)
{
    const fixed *optr = operands;
    /* Fetch num_operands before possible command merging. */
    static const byte op_num_operands[] = {
        cmd_segment_op_num_operands_values
    };
    int i = op_num_operands[op & 0xf];
    /* One picky compiler complains if we initialize to psw->cmd - 1. */
    byte *q = psw->cmd;

    --q;

#ifdef DEBUG
    if (gs_debug_c('L')) {
        int j;

        dmlprintf2(psw->cldev->memory, "[L]  %s:%d:", cmd_sub_op_names[op >> 4][op & 0xf],
                  (int)notes);
        for (j = 0; j < i; ++j)
            dmprintf1(psw->cldev->memory, " %g", fixed2float(operands[j]));
        dmputs(psw->cldev->memory, "\n");
    }
#endif

    /* Merge or shorten commands if possible. */
    if (op == cmd_opv_rlineto) {
        if (operands[0] == 0)
            op = cmd_opv_vlineto, optr = ++operands, i = 1;
        else if (operands[1] == 0)
            op = cmd_opv_hlineto, i = 1;
        else
            switch (*psw->dp) {
                case cmd_opv_rmoveto:
                    psw->delta_first.x = operands[0];
                    psw->delta_first.y = operands[1];
                    op = cmd_opv_rmlineto;
                  merge:cmd_uncount_op(*psw->dp, psw->len);
                    cmd_shorten_op(psw->cldev, psw->pcls, psw->len);	/* delete it */
                    q += psw->len - 1;
                    break;
                case cmd_opv_rmlineto:
                    if (notes != psw->notes)
                        break;
                    op = cmd_opv_rm2lineto;
                    goto merge;
                case cmd_opv_rm2lineto:
                    if (notes != psw->notes)
                        break;
                    if (operands[0] == -psw->delta_first.x &&
                        operands[1] == -psw->delta_first.y
                        ) {
                        cmd_uncount_op(cmd_opv_rm2lineto, psw->len);
                        *psw->dp = cmd_count_op(cmd_opv_rm3lineto, psw->len, psw->cldev->memory);
                        return 0;
                    }
                    break;
                default:
                    ;
            }
    }
    for (; --i >= 0; ++optr) {
        fixed d = *optr, d2;

        if (is_bits(d, _fixed_shift + 11) &&
            !(d & (float2fixed(0.25) - 1))
            ) {
            cmd_count_add1(stats_cmd_diffs[3]);
            d = ((d >> (_fixed_shift - 2)) & 0x1fff) + 0xc000;
            q += 2;
        } else if (is_bits(d, 19) && i > 0 && is_bits(d2 = optr[1], 19)) {
            cmd_count_add1(stats_cmd_diffs[0]);
            q[1] = (byte) ((d >> 13) & 0x3f);
            q[2] = (byte) (d >> 5);
            q[3] = (byte) ((d << 3) + ((d2 >> 16) & 7));
            q[4] = (byte) (d2 >> 8);
            q[5] = (byte) d2;
            q += 5;
            --i, ++optr;
            continue;
        } else if (is_bits(d, 22)) {
            cmd_count_add1(stats_cmd_diffs[1]);
            q[1] = (byte) (((d >> 16) & 0x3f) + 0x40);
            q += 3;
        } else if (is_bits(d, 30)) {
            cmd_count_add1(stats_cmd_diffs[2]);
            q[1] = (byte) (((d >> 24) & 0x3f) + 0x80);
            q[2] = (byte) (d >> 16);
            q += 4;
        } else {
            int b;

            cmd_count_add1(stats_cmd_diffs[4]);
            *++q = 0xe0;
            for (b = sizeof(fixed) - 1; b > 1; --b)
                *++q = (byte) (d >> (b * 8));
            q += 2;
        }
        q[-1] = (byte) (d >> 8);
        *q = (byte) d;
    }
    if (notes != psw->notes) {
        byte *dp;
        int code =
            set_cmd_put_op(&dp, psw->cldev, psw->pcls, cmd_opv_set_misc2, 3);

        if (code < 0)
            return code;
        dp[1] = segment_notes_known;
        dp[2] = notes;
        psw->notes = notes;
    } {
        int len = q + 2 - psw->cmd;
        byte *dp;
        int code = set_cmd_put_op(&dp, psw->cldev, psw->pcls, op, len);

        if (code < 0)
            return code;
        memcpy(dp + 1, psw->cmd, len - 1);
        psw->len = len;
        psw->dp = dp;
    }
    return 0;
}
/* Put out a line segment command. */
#define cmd_put_rmoveto(psw, operands)\
  cmd_put_segment(psw, cmd_opv_rmoveto, operands, sn_none)
#define cmd_put_rgapto(psw, operands, notes)\
  cmd_put_segment(psw, cmd_opv_rgapto, operands, notes)
#define cmd_put_rlineto(psw, operands, notes)\
  cmd_put_segment(psw, cmd_opv_rlineto, operands, notes)

/*
 * Write a path.  We go to a lot of trouble to omit segments that are
 * entirely outside the band.
 */
static int
cmd_put_path(gx_device_clist_writer * cldev, gx_clist_state * pcls,
             const gx_path * ppath, fixed ymin, fixed ymax, byte path_op,
             bool implicit_close, segment_notes keep_notes)
{
    gs_path_enum cenum;
    cmd_segment_writer writer;

    /*
     * initial_op is logically const.  We would like to declare it as
     * static const, since some systems really dislike non-const statics,
     * but this would entail a cast in set_first_point() that provokes a
     * warning message from gcc.  Instead, we pay the (tiny) cost of an
     * unnecessary dynamic initialization.
     */
    byte initial_op = cmd_opv_end_run;

    /*
     * We define the 'side' of a point according to its Y value as
     * follows:
     */
#define which_side(y) ((y) < ymin ? -1 : (y) >= ymax ? 1 : 0)

    /*
     * While writing a subpath, we need to keep track of any segments
     * skipped at the beginning of the subpath and any segments skipped
     * just before the current segment.  We do this with two sets of
     * state variables, one that tracks the actual path segments and one
     * that tracks the emitted segments.
     *
     * The following track the actual segments:
     */

    /*
     * The point and side of the last moveto (skipped if
     * start_side != 0):
     */
    gs_fixed_point start;
    int start_side = 0x7badf00d; /* Initialize against indeterminizm. */

    /*
     * Whether any lines or curves were skipped immediately
     * following the moveto:
     */
    bool start_skip = 0x7badf00d; /* Initialize against indeterminizm. */

    /* The side of the last point: */
    int side = 0x7badf00d; /* Initialize against indeterminizm. */

    /* The last point with side != 0: */
    gs_fixed_point out;

    /* If the last out-going segment was a lineto, */
    /* its notes: */
    segment_notes out_notes = 0x7badf00d; /* Initialize against indeterminizm. */

    /*
     * The following track the emitted segments:
     */

    /* The last point emitted: */
    fixed px = int2fixed(pcls->rect.x);
    fixed py = int2fixed(pcls->rect.y);

    /* The point of the last emitted moveto: */
    gs_fixed_point first;

    /* Information about the last emitted operation: */
    int open = 0;		/* -1 if last was moveto, 1 if line/curveto, */
                                /* 0 if newpath/closepath */
    struct { fixed vs[6]; } prev = { { 0 } };

    first.x = first.y = out.x = out.y = start.x = start.y = 0; /* Quiet gcc warning. */
    if_debug4m('p', cldev->memory, "[p]initial (%g,%g), clip [%g..%g)\n",
               fixed2float(px), fixed2float(py),
               fixed2float(ymin), fixed2float(ymax));
    gx_path_enum_init(&cenum, ppath);
    writer.cldev = cldev;
    writer.pcls = pcls;
    writer.notes = sn_none;
#define set_first_point() (writer.dp = &initial_op)
#define first_point() (writer.dp == &initial_op)
    set_first_point();
    for (;;) {
        fixed vs[6];

#define A vs[0]
#define B vs[1]
#define C vs[2]
#define D vs[3]
#define E vs[4]
#define F vs[5]
        int pe_op = gx_path_enum_next(&cenum, (gs_fixed_point *) vs);
        byte *dp;
        int code;

        switch (pe_op) {
            case 0:
                /* If the path is open and needs an implicit close, */
                /* do the close and then come here again. */
                if (open > 0 && implicit_close)
                    goto close;
                /* All done. */
                pcls->rect.x = fixed2int_var(px);
                pcls->rect.y = fixed2int_var(py);
                if_debug2m('p', cldev->memory, "[p]final (%d,%d)\n",
                           pcls->rect.x, pcls->rect.y);
                return set_cmd_put_op(&dp, cldev, pcls, path_op, 1);
            case gs_pe_moveto:
                /* If the path is open and needs an implicit close, */
                /* do a closepath and then redo the moveto. */
                if (open > 0 && implicit_close) {
                    gx_path_enum_backup(&cenum);
                    goto close;
                }
                open = -1;
                start.x = A, start.y = B;
                start_skip = false;
                if ((start_side = side = which_side(B)) != 0) {
                    out.x = A, out.y = B;
                    if_debug3m('p', cldev->memory, "[p]skip moveto (%g,%g) side %d\n",
                               fixed2float(out.x), fixed2float(out.y),
                               side);
                    continue;
                }
                C = A - px, D = B - py;
                first.x = px = A, first.y = py = B;
                code = cmd_put_rmoveto(&writer, &C);
                if_debug2m('p', cldev->memory, "[p]moveto (%g,%g)\n",
                           fixed2float(px), fixed2float(py));
                break;
            case gs_pe_gapto:
                {
                    int next_side = which_side(B);
                    segment_notes notes =
                    gx_path_enum_notes(&cenum) & keep_notes;

                    if (next_side == side && side != 0) {	/* Skip a line completely outside the clip region. */
                        if (open < 0)
                            start_skip = true;
                        out.x = A, out.y = B;
                        out_notes = notes;
                        if_debug3m('p', cldev->memory, "[p]skip gapto (%g,%g) side %d\n",
                                   fixed2float(out.x), fixed2float(out.y),
                                   side);
                        continue;
                    }
                    /* If we skipped any segments, put out a moveto/lineto. */
                    if (side && ((open < 0) || (px != out.x || py != out.y || first_point()))) {
                        C = out.x - px, D = out.y - py;
                        if (open < 0) {
                            first = out;
                            code = cmd_put_rmoveto(&writer, &C);
                        } else
                            code = cmd_put_rlineto(&writer, &C, out_notes);
                        if (code < 0)
                            return code;
                        px = out.x, py = out.y;
                        if_debug3m('p', cldev->memory, "[p]catchup %s (%g,%g) for line\n",
                                   (open < 0 ? "moveto" : "lineto"),
                                   fixed2float(px), fixed2float(py));
                    }
                    if ((side = next_side) != 0) {	/* Note a vertex going outside the clip region. */
                        out.x = A, out.y = B;
                    }
                    C = A - px, D = B - py;
                    px = A, py = B;
                    open = 1;
                    code = cmd_put_rgapto(&writer, &C, notes);
                }
                if_debug3m('p', cldev->memory, "[p]gapto (%g,%g) side %d\n",
                           fixed2float(px), fixed2float(py), side);
                break;
            case gs_pe_lineto:
                {
                    int next_side = which_side(B);
                    segment_notes notes =
                    gx_path_enum_notes(&cenum) & keep_notes;

                    if (next_side == side && side != 0) {	/* Skip a line completely outside the clip region. */
                        if (open < 0)
                            start_skip = true;
                        out.x = A, out.y = B;
                        out_notes = notes;
                        if_debug3m('p', cldev->memory, "[p]skip lineto (%g,%g) side %d\n",
                                   fixed2float(out.x), fixed2float(out.y),
                                   side);
                        continue;
                    }
                    /* If we skipped any segments, put out a moveto/lineto. */
                    if (side && ((open < 0) || (px != out.x || py != out.y || first_point()))) {
                        C = out.x - px, D = out.y - py;
                        if (open < 0) {
                            first = out;
                            code = cmd_put_rmoveto(&writer, &C);
                        } else
                            code = cmd_put_rlineto(&writer, &C, out_notes);
                        if (code < 0)
                            return code;
                        px = out.x, py = out.y;
                        if_debug3m('p', cldev->memory, "[p]catchup %s (%g,%g) for line\n",
                                   (open < 0 ? "moveto" : "lineto"),
                                   fixed2float(px), fixed2float(py));
                    }
                    if ((side = next_side) != 0) {	/* Note a vertex going outside the clip region. */
                        out.x = A, out.y = B;
                    }
                    C = A - px, D = B - py;
                    px = A, py = B;
                    open = 1;
                    code = cmd_put_rlineto(&writer, &C, notes);
                }
                if_debug3m('p', cldev->memory, "[p]lineto (%g,%g) side %d\n",
                           fixed2float(px), fixed2float(py), side);
                break;
            case gs_pe_closepath:
#ifdef DEBUG
                {
                    gs_path_enum cpenum;
                    gs_fixed_point cvs[3];
                    int op;

                    cpenum = cenum;
                    switch (op = gx_path_enum_next(&cpenum, cvs)) {
                        case 0:
                        case gs_pe_moveto:
                            break;
                        default:
                            mlprintf1(cldev->memory,
                                      "closepath followed by %d, not end/moveto!\n",
                                      op);
                    }
                }
#endif
                /* A closepath may require drawing an explicit line if */
                /* we skipped any segments at the beginning of the path. */
              close:if (side != start_side) {	/* If we skipped any segments, put out a moveto/lineto. */
                    if (side && (px != out.x || py != out.y || first_point())) {
                        C = out.x - px, D = out.y - py;
                        code = cmd_put_rlineto(&writer, &C, out_notes);
                        if (code < 0)
                            return code;
                        px = out.x, py = out.y;
                        if_debug2m('p', cldev->memory, "[p]catchup line (%g,%g) for close\n",
                                   fixed2float(px), fixed2float(py));
                    }
                    if (open > 0 && start_skip) {	/* Draw the closing line back to the start. */
                        C = start.x - px, D = start.y - py;
                        code = cmd_put_rlineto(&writer, &C, sn_none);
                        if (code < 0)
                            return code;
                        px = start.x, py = start.y;
                        if_debug2m('p', cldev->memory, "[p]draw close to (%g,%g)\n",
                                   fixed2float(px), fixed2float(py));
                    }
                }
                /*
                 * We don't bother to update side because we know that the
                 * next element after a closepath, if any, must be a moveto.
                 * We must handle explicitly the possibility that the entire
                 * subpath was skipped.
                 */
                if (implicit_close || open <= 0) {
                    /*
                     * Force writing an explicit moveto if the next subpath
                     * starts with a moveto to the same point where this one
                     * ends.
                     */
                    set_first_point();
                    /*
                     * If implicit_close == true, we don't need an explicit closepath,
                     * because the filling algorithm will close subpath automatically.
                     * Otherwise, if open < 0, we have an empty closed path.
                     * If side != 0, it is outside the band, so we can
                     * safely skip it, because the band has been expanded
                     * with line width.
                     */
                    if (side != 0) {
                        open = 0;
                        continue;
                    }
                }
                open = 0;
                px = first.x, py = first.y;
                code = cmd_put_segment(&writer, cmd_opv_closepath, &A, sn_none);
                if_debug0m('p', cldev->memory, "[p]close\n");
                break;
            case gs_pe_curveto:
                {
                    segment_notes notes =
                    gx_path_enum_notes(&cenum) & keep_notes;

                    {
                        fixed bpy, bqy;
                        int all_side, out_side;

                        /* Compute the Y bounds for the clipping check. */
                        if (B < D)
                            bpy = B, bqy = D;
                        else
                            bpy = D, bqy = B;
                        if (F < bpy)
                            bpy = F;
                        else if (F > bqy)
                            bqy = F;
                        all_side = (bqy < ymin ? -1 : bpy > ymax ? 1 : 0);
                        if (all_side != 0) {
                            if (all_side == side) {	/* Skip a curve entirely outside the clip region. */
                                if (open < 0)
                                    start_skip = true;
                                out.x = E, out.y = F;
                                out_notes = notes;
                                if_debug3m('p', cldev->memory,
                                           "[p]skip curveto (%g,%g) side %d\n",
                                           fixed2float(out.x), fixed2float(out.y),
                                           side);
                                continue;
                            }
                            out_side = all_side;
                        } else
                            out_side = which_side(F);
                        /* If we skipped any segments, put out a moveto/lineto. */
                        if (side && ((open < 0) || (px != out.x || py != out.y || first_point()))) {
                            fixed diff[2];

                            diff[0] = out.x - px, diff[1] = out.y - py;
                            if (open < 0) {
                                first = out;
                                code = cmd_put_rmoveto(&writer, diff);
                            } else
                                code = cmd_put_rlineto(&writer, diff, out_notes);
                            if (code < 0)
                                return code;
                            px = out.x, py = out.y;
                            if_debug3m('p', cldev->memory,
                                       "[p]catchup %s (%g,%g) for curve\n",
                                       (open < 0 ? "moveto" : "lineto"),
                                       fixed2float(px), fixed2float(py));
                        }
                        if ((side = out_side) != 0) {	/* Note a vertex going outside the clip region. */
                            out.x = E, out.y = F;
                        }
                    }
                    {
                        fixed nx = E, ny = F;
                        const fixed *optr = vs;
                        byte op;

                        if_debug7m('p', cldev->memory,
                                   "[p]curveto (%g,%g; %g,%g; %g,%g) side %d\n",
                                   fixed2float(A), fixed2float(B),
                                   fixed2float(C), fixed2float(D),
                                   fixed2float(E), fixed2float(F), side);
                        E -= C, F -= D;
                        C -= A, D -= B;
                        A -= px, B -= py;
                        if (*writer.dp >= cmd_opv_min_curveto &&
                            *writer.dp <= cmd_opv_max_curveto &&
                            ((prev.A == 0 &&
                              A == prev.E && C == prev.C && E == prev.A &&
                              B == -prev.F && D == -prev.D && F == -prev.B) ||
                             (prev.A != 0 &&
                              A == -prev.E && C == -prev.C && E == -prev.A &&
                              B == prev.F && D == prev.D && F == prev.B))
                            )
                            op = cmd_opv_scurveto;
                        else if (A == 0 && F == 0) {
                            optr++, op = cmd_opv_vhcurveto;
                            if ((B ^ C) >= 0) {
                                if (D == C && E == B)
                                    op = cmd_opv_vqcurveto;
                            } else if (D == -C && E == -B)
                                op = cmd_opv_vqcurveto;
                        } else if (B == 0 && E == 0) {
                            B = A, E = F, optr++, op = cmd_opv_hvcurveto;
                            if ((B ^ D) >= 0) {
                                if (C == D && E == B)
                                    op = cmd_opv_hqcurveto;
                            } else if (C == -D && E == -B)
                                C = D, op = cmd_opv_hqcurveto;
                        }
                        else if (A == 0 && B == 0)
                            optr += 2, op = cmd_opv_nrcurveto;
                        else if (E == 0 && F == 0)
                            op = cmd_opv_rncurveto;
                        else
                            op = cmd_opv_rrcurveto;
                        memcpy(prev.vs, vs, sizeof(prev.vs));
                        px = nx, py = ny;
                        open = 1;
                        code = cmd_put_segment(&writer, op, optr, notes);
                    }
                }
                break;
            default:
                return_error(gs_error_rangecheck);
        }
        if (code < 0)
            return code;
#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
    }
}
