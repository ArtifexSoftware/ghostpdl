/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* PDF 1.4 blending functions */

#include "memory_.h"
#include "gx.h"
#include "gp.h"
#include "gstparam.h"
#include "gsrect.h"
#include "gxblend.h"
#include "gxdcconv.h"
#include "gxdevcli.h"
#include "gxistate.h"
#include "gdevdevn.h"
#include "gdevp14.h"
#include "vdtrace.h"
#include "gxdcconv.h"
#include "gsicc_cache.h"

#ifdef DUMP_TO_PNG
#include "png_.h"
#endif

/*
 * Unpack a device color.  This routine is similar to the device's
 * decode_color procedure except for two things.  The procedure produces 1
 * byte values instead of gx_color_values (2 bytes).  A separate
 * procedure is used instead of the decode_color to minimize execution time.
 */
void
pdf14_unpack_additive(int num_comp, gx_color_index color,
                                pdf14_device * p14dev, byte * out)
{
    int i;

    for (i = num_comp - 1; i >= 0; i--) {
        out[i] = (byte)(color & 0xff);
        color >>= 8;
    }
}

/*
 * Unpack a device color.  This routine is similar to the device's
 * decode_color procedure except for two things.  The procedure produces 1
 * byte values instead of gx_color_values (2 bytes) and the output values
 * are inverted for subtractive color spaces (like CMYK).  A separate
 * procedure is used instead of the decode_color to minimize execution time.
 */
void
pdf14_unpack_subtractive(int num_comp, gx_color_index color,
                                pdf14_device * p14dev, byte * out)
{
    int i;

    for (i = num_comp - 1; i >= 0; i--) {
        out[i] = 0xff - (byte)(color & 0xff);
        color >>= 8;
    }
}

/*
 * Unpack a device color.  This routine is used for devices in which we do
 * not know the details of the process color model.  In this case we use
 * the device's decode_color procedure.
 */
void
pdf14_unpack_custom(int num_comp, gx_color_index color,
                                pdf14_device * p14dev, byte * out)
{
    int i;
    gx_device * tdev = p14dev->target;
    gx_color_value cm_values[GX_DEVICE_COLOR_MAX_COMPONENTS];

    dev_proc(tdev, decode_color)(tdev, color, cm_values);
    for (i = 0; i < num_comp; i++)
        out[i] = 0xff - gx_color_value_to_byte(cm_values[i]);
}

#if RAW_DUMP
extern unsigned int global_index;
#endif

static void 
copy_plane_part(byte *des_ptr, int des_rowstride, byte *src_ptr, int src_rowstride, 
                int width, int height)
{
    int y; 

    for (y = 0; y < height; ++y) {
        memcpy(des_ptr, src_ptr, width);
        des_ptr += des_rowstride;
        src_ptr += src_rowstride;
    }
}

static void
copy_extra_planes(byte *des_buf, pdf14_buf *des_info, byte *src_buf, 
                  pdf14_buf *src_info, int width, int height)
{
    /* alpha_g and shape do not copy */
    des_buf += des_info->planestride * ((des_info->has_shape ? 1 : 0) +
                                        (des_info->has_alpha_g ? 1 : 0));
    src_buf += src_info->planestride * ((src_info->has_shape ? 1 : 0) +
                                        (src_info->has_alpha_g ? 1 : 0));
    /* tags plane does copy */
    if (des_info->has_tags) {
        if (src_info->has_tags) {
            copy_plane_part(des_buf, des_info->rowstride, src_buf, 
                            src_info->rowstride, width, height);
        } 
    }
}

int 
pdf14_preserve_backdrop_cm(pdf14_buf *buf, cmm_profile_t *group_profile, 
                           pdf14_buf *tos, cmm_profile_t *tos_profile, 
                           gs_memory_t *memory, gs_imager_state *pis, gx_device *dev,
                           bool knockout_buff)
{
    /* Make copy of backdrop, but convert to new group's colorspace */
    int x0 = max(buf->rect.p.x, tos->rect.p.x);
    int x1 = min(buf->rect.q.x, tos->rect.q.x);
    int y0 = max(buf->rect.p.y, tos->rect.p.y);
    int y1 = min(buf->rect.q.y, tos->rect.q.y);

    if (x0 < x1 && y0 < y1) {
        int width = x1 - x0;
        int height = y1 - y0;
        byte *buf_plane, *tos_plane;
        gsicc_rendering_param_t rendering_params;
        gsicc_link_t *icc_link;
        gsicc_bufferdesc_t input_buff_desc;
        gsicc_bufferdesc_t output_buff_desc;

        /* Define the rendering intents */
        rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
        rendering_params.graphics_type_tag = GS_IMAGE_TAG;
        rendering_params.override_icc = false;
        rendering_params.preserve_black = gsBKPRESNOTSPECIFIED;
        rendering_params.rendering_intent = gsPERCEPTUAL;
        rendering_params.cmm = gsCMM_DEFAULT;
        /* Request the ICC link for the transform that we will need to use */
        icc_link = gsicc_get_link_profile(pis, dev, tos_profile, group_profile,
                                          &rendering_params, memory, false);
        if (icc_link == NULL)
            return gs_throw(gs_error_unknownerror, "ICC link failed.  Trans backdrop");

        if (icc_link->is_identity) {
            pdf14_preserve_backdrop(buf, tos, knockout_buff);
            gsicc_release_link(icc_link);
            return 0;
        } else {
            if (knockout_buff) {
                buf_plane = buf->backdrop + x0 - buf->rect.p.x + 
                        (y0 - buf->rect.p.y) * buf->rowstride;
                tos_plane = tos->backdrop + x0 - tos->rect.p.x + 
                        (y0 - tos->rect.p.y) * tos->rowstride;
                memset(buf->backdrop, 0, buf->n_chan * buf->planestride);
            } else {
                buf_plane = buf->data + x0 - buf->rect.p.x + 
                        (y0 - buf->rect.p.y) * buf->rowstride;
                tos_plane = tos->data + x0 - tos->rect.p.x + 
                        (y0 - tos->rect.p.y) * tos->rowstride;
                /* First clear out everything. There are cases where the incoming buf
                   has a region outside the existing tos group.  Need to check if this
                   is getting clipped in which case we need to fix the allocation of
                   the buffer to be smaller */
                memset(buf->data, 0, buf->n_planes * buf->planestride);
            }
            /* Set up the buffer descriptors. */
            gsicc_init_buffer(&input_buff_desc, tos_profile->num_comps, 1, false, 
                              false, true, tos->planestride, tos->rowstride, height, 
                              width);
            gsicc_init_buffer(&output_buff_desc, group_profile->num_comps, 1, false, 
                              false, true, buf->planestride, buf->rowstride, height, 
                              width);
            /* Transform the data.  */
            (icc_link->procs.map_buffer)(dev, icc_link, &input_buff_desc,
                                         &output_buff_desc, tos_plane, buf_plane);
            gsicc_release_link(icc_link);
        }
        /* Copy the alpha data */
        buf_plane += buf->planestride * (buf->n_chan - 1);
        tos_plane += tos->planestride * (tos->n_chan - 1);
        copy_plane_part(buf_plane, buf->rowstride, tos_plane, tos->rowstride, width, 
                        height);
        buf_plane += buf->planestride;
        tos_plane += tos->planestride;

        if (!knockout_buff)
            copy_extra_planes(buf_plane, buf, tos_plane, tos, width, height); 
    }
#if RAW_DUMP
    if (x0 < x1 && y0 < y1) {
        byte *buf_plane = buf->data + x0 - buf->rect.p.x +
            (y0 - buf->rect.p.y) * buf->rowstride;
        dump_raw_buffer(y1 - y0, x1 - x0, buf->n_planes, buf->planestride, 
                        buf->rowstride, "BackDropInit_CM", buf_plane);
        global_index++;
    }
#endif
    return 0;
}

void
pdf14_preserve_backdrop(pdf14_buf *buf, pdf14_buf *tos, bool knockout_buff)
{
    /* make copy of backdrop for compositing */
    int x0 = max(buf->rect.p.x, tos->rect.p.x);
    int x1 = min(buf->rect.q.x, tos->rect.q.x);
    int y0 = max(buf->rect.p.y, tos->rect.p.y);
    int y1 = min(buf->rect.q.y, tos->rect.q.y);

    if (x0 < x1 && y0 < y1) {
        int width = x1 - x0;
        int height = y1 - y0;
        byte *buf_plane, *tos_plane;
        int i;

        if (knockout_buff) {
            buf_plane = buf->backdrop + x0 - buf->rect.p.x + 
                    (y0 - buf->rect.p.y) * buf->rowstride;
            tos_plane = tos->backdrop + x0 - tos->rect.p.x + 
                    (y0 - tos->rect.p.y) * tos->rowstride;
            memset(buf->backdrop, 0, buf->n_chan * buf->planestride);
        } else {
            buf_plane = buf->data + x0 - buf->rect.p.x + 
                    (y0 - buf->rect.p.y) * buf->rowstride;
            tos_plane = tos->data + x0 - tos->rect.p.x + 
                    (y0 - tos->rect.p.y) * tos->rowstride;
            /* First clear out everything. There are cases where the incoming buf
               has a region outside the existing tos group.  Need to check if this
               is getting clipped in which case we need to fix the allocation of
               the buffer to be smaller */
            memset(buf->data, 0, buf->n_planes * buf->planestride);
        }
        /* Color and alpha plane */
        for (i = 0; i < tos->n_chan; i++) {
            copy_plane_part(buf_plane, buf->rowstride, tos_plane, tos->rowstride, 
                            width, height);
            buf_plane += buf->planestride;
            tos_plane += tos->planestride;
        }
        if (!knockout_buff)
            copy_extra_planes(buf_plane, buf, tos_plane, tos, width, height); 
    }
#if RAW_DUMP
    if (x0 < x1 && y0 < y1) {
        byte *buf_plane = buf->data + x0 - buf->rect.p.x +
            (y0 - buf->rect.p.y) * buf->rowstride;
        dump_raw_buffer(y1 - y0, x1 - x0, buf->n_planes, buf->planestride, 
                        buf->rowstride, "BackDropInit", buf_plane);
        global_index++;
    }
#endif
}

void
pdf14_compose_group(pdf14_buf *tos, pdf14_buf *nos, pdf14_buf *maskbuf,
              int x0, int x1, int y0, int y1, int n_chan, bool additive,
              const pdf14_nonseparable_blending_procs_t * pblend_procs,
              bool overprint, gx_color_index drawn_comps, bool blendspot,
              gs_memory_t *memory)
{
    byte alpha = tos->alpha;
    byte shape = tos->shape;
    gs_blend_mode_t blend_mode = tos->blend_mode;
    byte *tos_ptr = tos->data + x0 - tos->rect.p.x +
        (y0 - tos->rect.p.y) * tos->rowstride;
    byte *nos_ptr = nos->data + x0 - nos->rect.p.x +
        (y0 - nos->rect.p.y) * nos->rowstride;
    byte *mask_row_ptr = NULL;
    byte *mask_curr_ptr = NULL;
    int tos_planestride = tos->planestride;
    int nos_planestride = nos->planestride;
    byte mask_bg_alpha = 0; /* Quiet compiler. */
    int width = x1 - x0;
    int x, y;
    int i;
    byte tos_pixel[PDF14_MAX_PLANES];
    byte nos_pixel[PDF14_MAX_PLANES];
    byte back_drop[PDF14_MAX_PLANES];
    bool tos_isolated = tos->isolated;
    bool nos_isolated = nos->isolated;
    bool nos_knockout = nos->knockout;
    byte *nos_alpha_g_ptr;
    int tos_shape_offset = n_chan * tos_planestride;
    int tos_alpha_g_offset = tos_shape_offset + (tos->has_shape ? tos_planestride : 0);
    bool tos_has_tag = tos->has_tags;
    int tos_tag_offset = tos_planestride * (tos->n_planes - 1);
    int nos_shape_offset = n_chan * nos_planestride;
    int nos_alpha_g_offset = nos_shape_offset + (nos->has_shape ? nos_planestride : 0);
    int nos_tag_offset = nos_planestride * (nos->n_planes - 1);
    byte *mask_tr_fn = NULL; /* Quiet compiler. */
    gx_color_index comps;
    bool has_mask = false;
    bool in_mask_rect_y = false;
    bool in_mask_rect = false;
    byte pix_alpha;
    bool has_backdrop = false;
    byte *backdrop_ptr = NULL; /* Quiet compiler. */
#if RAW_DUMP
    byte *composed_ptr = NULL;
#endif

    if ((tos->n_chan == 0) || (nos->n_chan == 0))
        return;
    rect_merge(nos->dirty, tos->dirty);
    if_debug6m('v', memory,
               "pdf14_pop_transparency_group y0 = %d, y1 = %d, w = %d, alpha = %d, shape = %d, tag =  bm = %d\n",
               y0, y1, width, alpha, shape, blend_mode);
    if (!nos->has_shape)
        nos_shape_offset = 0;
    if (!nos->has_tags)
        nos_tag_offset = 0;
    if (nos->has_alpha_g) {
        nos_alpha_g_ptr = nos_ptr + nos_alpha_g_offset;
    } else
        nos_alpha_g_ptr = NULL;
    if (nos->backdrop != NULL) {
        has_backdrop = true;
        backdrop_ptr = nos->backdrop + x0 - nos->rect.p.x +
                       (y0 - nos->rect.p.y) * nos->rowstride;
    }

    if (maskbuf != NULL) {
        int tmp;

        mask_tr_fn = maskbuf->transfer_fn;
        /* Make sure we are in the mask buffer */
        if (maskbuf->data != NULL) {
            mask_row_ptr = maskbuf->data + x0 - maskbuf->rect.p.x +
                    (y0 - maskbuf->rect.p.y) * maskbuf->rowstride;
            has_mask = true;
        } 
        /* We may have a case, where we are outside the maskbuf rect. */
        /* We would have avoided creating the maskbuf->data */
        /* In that case, we should use the background alpha value */
        /* See discussion on the BC entry in the PDF spec.   */
        mask_bg_alpha = maskbuf->alpha;
        /* Adjust alpha by the mask background alpha.   This is only used
           if we are outside the soft mask rect during the filling operation */
        mask_bg_alpha = mask_tr_fn[mask_bg_alpha];
        tmp = alpha * mask_bg_alpha + 0x80;
        mask_bg_alpha = (tmp + (tmp >> 8)) >> 8;
    }
    n_chan--;
#if RAW_DUMP
    composed_ptr = nos_ptr;
    dump_raw_buffer(y1-y0, width, tos->n_planes, tos_planestride, tos->rowstride,
                    "bImageTOS",tos_ptr);
    dump_raw_buffer(y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "cImageNOS",nos_ptr);
    if (maskbuf !=NULL && maskbuf->data != NULL) {
        dump_raw_buffer(maskbuf->rect.q.y - maskbuf->rect.p.y, 
                        maskbuf->rect.q.x - maskbuf->rect.p.x, maskbuf->n_planes,
                        maskbuf->planestride, maskbuf->rowstride, "dMask", 
                        maskbuf->data);
    }
#endif
    for (y = y1 - y0; y > 0; --y) {
        mask_curr_ptr = mask_row_ptr;
        if (has_mask && y1 - y >= maskbuf->rect.p.y && y1 - y < maskbuf->rect.q.y) {
            in_mask_rect_y = true;
        } else {
            in_mask_rect_y = false;
        }
        for (x = 0; x < width; x++) {
            if (in_mask_rect_y && has_mask && x0 + x >= maskbuf->rect.p.x && 
                x0 + x < maskbuf->rect.q.x) {
                in_mask_rect = true;
            } else {
                in_mask_rect = false;
            }
            pix_alpha = alpha;
            /* If we have a soft mask, then we have some special handling of the 
               group alpha value */
            if (maskbuf != NULL) {
                if (!in_mask_rect) {
                    /* Special case where we have a soft mask but are outside the
                       range of the soft mask and must use the background alpha value */
                    pix_alpha = mask_bg_alpha;
                } else {
                    /* If we are isolated, do not double apply the alpha */
                    if (tos_isolated) {
                        pix_alpha = 0xff;
                    }
                }
            }
             /* Complement the components for subtractive color spaces */
            if (additive) {
                for (i = 0; i <= n_chan; ++i) {
                    tos_pixel[i] = tos_ptr[i * tos_planestride];
                    nos_pixel[i] = nos_ptr[i * nos_planestride];
                }
            } else {
                for (i = 0; i < n_chan; ++i) {
                    tos_pixel[i] = 255 - tos_ptr[i * tos_planestride];
                    nos_pixel[i] = 255 - nos_ptr[i * nos_planestride];
                }
                tos_pixel[n_chan] = tos_ptr[n_chan * tos_planestride];
                nos_pixel[n_chan] = nos_ptr[n_chan * nos_planestride];
            }
            if (mask_curr_ptr != NULL) {
                if (in_mask_rect) {
                    byte mask = mask_tr_fn[*mask_curr_ptr++];
                    int tmp = pix_alpha * mask + 0x80;
                    pix_alpha = (tmp + (tmp >> 8)) >> 8;
                } else {
                    mask_curr_ptr++;
                }
            }
#		    if VD_PAINT_MASK
                    vd_pixel(int2fixed(x), int2fixed(y1-y), mask);
#		    endif
                
            if (nos_knockout) {
                /* We need to be knocking out what ever is on the nos, but may need to combine with it's backdrop */
                byte *nos_shape_ptr = nos_shape_offset ?
                    &nos_ptr[nos_shape_offset] : NULL;
                byte *nos_tag_ptr = nos_tag_offset ?
                    &nos_ptr[nos_tag_offset] : NULL;
                byte tos_shape, tos_tag;

                if (tos->has_shape) 
                    tos_shape = tos_ptr[tos_shape_offset];
                else
                    tos_shape = 255;
                if (tos->has_tags)
                    tos_tag = tos_ptr[tos_tag_offset];
                else
                    tos_tag = 255;

                if (nos_isolated) {
                    /* We do not need to compose with the backdrop */
                    art_pdf_composite_knockout_isolated_8(nos_pixel, nos_shape_ptr,
                                                          nos_tag_ptr, tos_pixel,
                                                          n_chan, tos_shape, tos_tag, 
                                                          pix_alpha, shape, has_mask);
                } else {  
                    /* Per the PDF spec, since the tos is not isolated and we are 
                       going onto a knock out group, we do the composition with
                       the nos initial backdrop. */
                    if (additive) {
                        for (i = 0; i <= n_chan; ++i) {
                            back_drop[i] = backdrop_ptr[i * nos_planestride];
                        }
                    } else {
                        for (i = 0; i < n_chan; ++i) {
                            back_drop[i] = 255 - backdrop_ptr[i * nos_planestride];
                        }
                        back_drop[n_chan] = backdrop_ptr[n_chan * nos_planestride];
                    }
                    art_pdf_composite_knockout_group_8(back_drop, tos_shape, 
                                                       nos_pixel, nos_alpha_g_ptr, 
                                                       tos_pixel, n_chan, pix_alpha, 
                                                       blend_mode, pblend_procs);
                }
            } else {
                if (tos_isolated) {
                    art_pdf_composite_group_8(nos_pixel, nos_alpha_g_ptr, tos_pixel, 
                                              n_chan, pix_alpha, blend_mode, 
                                              pblend_procs);
                } else {
                    byte tos_alpha_g = tos_ptr[tos_alpha_g_offset];
                    art_pdf_recomposite_group_8(nos_pixel, nos_alpha_g_ptr,
                                        tos_pixel, tos_alpha_g, n_chan,
                                        pix_alpha, blend_mode, pblend_procs);
                }
                if (tos_has_tag) {
                    if (pix_alpha == 255) {
                        nos_ptr[nos_tag_offset] = tos_ptr[tos_tag_offset];
                    } else if (pix_alpha != 0 && tos_ptr[tos_tag_offset] !=
                               GS_UNTOUCHED_TAG) {
                        nos_ptr[nos_tag_offset] =
                            (nos_ptr[nos_tag_offset] |
                            tos_ptr[tos_tag_offset]) &
                            ~GS_UNTOUCHED_TAG;
                    }
                }
            }
            if (nos_shape_offset) {
                nos_ptr[nos_shape_offset] =
                    art_pdf_union_mul_8 (nos_ptr[nos_shape_offset],
                                            tos_ptr[tos_shape_offset],
                                            shape);
            }
            /* Complement the results for subtractive color spaces */
            if (additive) {
                for (i = 0; i <= n_chan; ++i) {
                    nos_ptr[i * nos_planestride] = nos_pixel[i];
                }
            } else {
                if (overprint) {
                    for (i = 0, comps = drawn_comps; comps != 0; ++i, comps >>= 1) {
                        if ((comps & 0x1) != 0) {
                            nos_ptr[i * nos_planestride] = 255 - nos_pixel[i];
                        }
                    }
                } else {
                    for (i = 0; i < n_chan; ++i)
                        nos_ptr[i * nos_planestride] = 255 - nos_pixel[i];
                }
                nos_ptr[n_chan * nos_planestride] = nos_pixel[n_chan];
            }
#		if VD_PAINT_COLORS
                vd_pixel(int2fixed(x), int2fixed(y1-y), n_chan == 0 ?
                    (nos_pixel[0] << 16) + (nos_pixel[0] << 8) + nos_pixel[0] :
                    (nos_pixel[0] << 16) + (nos_pixel[1] << 8) + nos_pixel[2]);
#		endif
#		if VD_PAINT_ALPHA
                vd_pixel(int2fixed(x), int2fixed(y1-y),
                    (nos_pixel[n_chan] << 16) + (nos_pixel[n_chan] << 8) +
                     nos_pixel[n_chan]);
#		endif
            if (nos_alpha_g_ptr != NULL)
                ++nos_alpha_g_ptr;
            if (has_backdrop)
                ++backdrop_ptr;
            ++tos_ptr;
            ++nos_ptr;
        }
        tos_ptr += tos->rowstride - width;
        nos_ptr += nos->rowstride - width;
        if (nos_alpha_g_ptr != NULL)
            nos_alpha_g_ptr += nos->rowstride - width;
        if (mask_row_ptr != NULL)
            mask_row_ptr += maskbuf->rowstride;
        if (has_backdrop)
            backdrop_ptr += nos->rowstride - width;
    }
#if RAW_DUMP
    dump_raw_buffer(y1-y0, width, nos->n_planes, nos_planestride, nos->rowstride,
                    "eComposed", composed_ptr);
    global_index++;
#endif
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
 */
gx_color_index
pdf14_encode_color(gx_device *dev, const gx_color_value	colors[])
{
    gx_color_index color = 0;
    uchar i;
    uchar ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(8);
    for (i = 0; i < ncomp; i++) {
        color <<= 8;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Encode a list of colorant values into a gx_color_index_value.
   Stick the tag information at the end.
 */
gx_color_index
pdf14_encode_color_tag(gx_device *dev, const gx_color_value colors[])
{
    gx_color_index color;
    uchar i;
    uchar ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(8);
    /* Add in the tag information */
    color = dev->graphics_type_tag & ~GS_DEVICE_ENCODES_TAGS;
    for (i = 0; i < ncomp; i++) {
        color <<= 8;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

/*
 * Decode a gx_color_index value back to a list of colorant values.
 */
int
pdf14_decode_color(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    uchar i;
    uchar ncomp = dev->color_info.num_components;

    for (i = 0; i < ncomp; i++) {
        out[ncomp - i - 1] = (gx_color_value) ((color & 0xff) * 0x101);
        color >>= 8;
    }
    return 0;
}

void
pdf14_gray_cs_to_cmyk_cm(gx_device * dev, frac gray, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
    for (--num_comp; num_comp > 3; num_comp--)
        out[num_comp] = 0;
}

/*
 * Default map from DeviceRGB color space to DeviceCMYK color
 * model. Since this mapping is defined by the PostScript language
 * it is unlikely that any device with a DeviceCMYK color model
 * would define this mapping on its own.
 *
 * If the imager state is not available, map as though the black
 * generation and undercolor removal functions are identity
 * transformations. This mode is used primarily to support the
 * raster operation (rop) feature of PCL, which requires that
 * the raster operation be performed in an RGB color space.
 * Note that default black generation and undercolor removal
 * functions in PostScript need NOT be identity transformations:
 * often they are { pop 0 }.
 */
void
pdf14_rgb_cs_to_cmyk_cm(gx_device * dev, const gs_imager_state *pis,
                           frac r, frac g, frac b, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    if (pis != 0)
        color_rgb_to_cmyk(r, g, b, pis, out, dev->memory);
    else {
        frac    c = frac_1 - r, m = frac_1 - g, y = frac_1 - b;
        frac    k = min(c, min(m, y));

        out[0] = c - k;
        out[1] = m - k;
        out[2] = y - k;
        out[3] = k;
    }
    for (--num_comp; num_comp > 3; num_comp--)
        out[num_comp] = 0;
}

void
pdf14_cmyk_cs_to_cmyk_cm(gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = c;
    out[1] = m;
    out[2] = y;
    out[3] = k;
    for (--num_comp; num_comp > 3; num_comp--)
        out[num_comp] = 0;
}

#ifdef DUMP_TO_PNG
/* Dumps a planar RGBA image to	a PNG file. */
static	int
dump_planar_rgba(gs_memory_t *mem, const pdf14_buf *pbuf)
{
    int rowstride = pbuf->rowstride, planestride = pbuf->planestride;
    int rowbytes = width << 2;
    gs_int_rect rect = buf->rect;
    int x1 = min(pdev->width, rect.q.x);
    int y1 = min(pdev->height, rect.q.y);
    int width = x1 - rect.p.x;
    int height = y1 - rect.p.y;
    byte *buf_ptr = buf->data + rect.p.y * buf->rowstride + rect.p.x;
    byte *row = gs_malloc(mem, rowbytes, 1, "png raster buffer");
    png_struct *png_ptr =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info *info_ptr =
    png_create_info_struct(png_ptr);
    const char *software_key = "Software";
    char software_text[256];
    png_text text_png;
    FILE *file;
    int code;
    int y;

    if (buf->data == NULL)
        return 0;

    file = gp_fopen ("c:\\temp\\tmp.png", "wb");

    if_debug0m('v', mem, "[v]pnga_output_page\n");

    if (row == 0 || png_ptr == 0 || info_ptr == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    /* set error handling */
    if (setjmp(png_ptr->jmpbuf)) {
        /* If we get here, we had a problem reading the file */
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }

    code = 0;			/* for normal path */
    /* set up the output control */
    png_init_io(png_ptr, file);

    /* set the file information here */
    info_ptr->width = width;
    info_ptr->height = height;
    /* resolution is in pixels per meter vs. dpi */
    info_ptr->x_pixels_per_unit =
        (png_uint_32) (96.0 * (100.0 / 2.54));
    info_ptr->y_pixels_per_unit =
        (png_uint_32) (96.0 * (100.0 / 2.54));
    info_ptr->phys_unit_type = PNG_RESOLUTION_METER;
    info_ptr->valid |= PNG_INFO_pHYs;

    /* At present, only supporting 32-bit rgba */
    info_ptr->bit_depth = 8;
    info_ptr->color_type = PNG_COLOR_TYPE_RGB_ALPHA;

    /* add comment */
    gs_sprintf(software_text, "%s %d.%02d", gs_product,
            (int)(gs_revision / 100), (int)(gs_revision % 100));
    text_png.compression = -1;	/* uncompressed */
    text_png.key = (char *)software_key;	/* not const, unfortunately */
    text_png.text = software_text;
    text_png.text_length = strlen(software_text);
    info_ptr->text = &text_png;
    info_ptr->num_text = 1;

    /* write the file information */
    png_write_info(png_ptr, info_ptr);

    /* don't write the comments twice */
    info_ptr->num_text = 0;
    info_ptr->text = NULL;

    /* Write the contents of the image. */
    for (y = 0; y < height; ++y) {
        int x;

        for (x = 0; x < width; ++x) {
            row[(x << 2)] = buf_ptr[x];
            row[(x << 2) + 1] = buf_ptr[x + planestride];
            row[(x << 2) + 2] = buf_ptr[x + planestride * 2];
            row[(x << 2) + 3] = buf_ptr[x + planestride * 3];
        }
        png_write_row(png_ptr, row);
        buf_ptr += rowstride;
    }

    /* write the rest of the file */
    png_write_end(png_ptr, info_ptr);

  done:
    /* free the structures */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    gs_free(mem, row, rowbytes, 1, "png raster buffer");

    fclose (file);
    return code;
}
#endif

void
gx_build_blended_image_row(byte *buf_ptr, int y, int planestride,
                           int width, int num_comp, byte bg, byte *linebuf)
{
    int x;
    for (x = 0; x < width; x++) {
        byte comp, a;
        int tmp, comp_num;

        /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
        a = buf_ptr[x + planestride * num_comp];

        if ((a + 1) & 0xfe) {
            a ^= 0xff;
            for (comp_num = 0; comp_num < num_comp; comp_num++) {
                comp  = buf_ptr[x + planestride * comp_num];
                tmp = ((bg - comp) * a) + 0x80;
                comp += (tmp + (tmp >> 8)) >> 8;
                linebuf[x * num_comp + comp_num] = comp;
            }
        } else if (a == 0) {
            for (comp_num = 0; comp_num < num_comp; comp_num++) {
                linebuf[x * num_comp + comp_num] = bg;
            }
        } else {
            for (comp_num = 0; comp_num < num_comp; comp_num++) {
                comp = buf_ptr[x + planestride * comp_num];
                linebuf[x * num_comp + comp_num] = comp;
            }
        }
    }
}

void
gx_blend_image_buffer(byte *buf_ptr, int width, int height, int rowstride,
                      int planestride, int num_comp, byte bg)
{
    int x, y;
    int position;
    byte comp, a;
    int tmp, comp_num;

    for (y = 0; y < height; y++) {
        position = y * rowstride;
        for (x = 0; x < width; x++) {
            /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
            a = buf_ptr[position + planestride * num_comp];
            if ((a + 1) & 0xfe) {
                a ^= 0xff;
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    comp  = buf_ptr[position + planestride * comp_num];
                    tmp = ((bg - comp) * a) + 0x80;
                    comp += (tmp + (tmp >> 8)) >> 8;
                    buf_ptr[position + planestride * comp_num] = comp;
                }
            } else if (a == 0) {
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    buf_ptr[position + planestride * comp_num] = bg;
                }
            }
            position+=1;
        }
    }
}

int
gx_put_blended_image_cmykspot(gx_device *target, byte *buf_ptr,
                      int planestride, int rowstride,
                      int x0, int y0, int width, int height, int num_comp, byte bg,
                      bool has_tags, gs_int_rect rect, gs_separations * pseparations)
{
    int code = 0;
    int x, y, tmp, comp_num, output_comp_num;
    gx_color_index color;
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value comp;
    byte a;
    int input_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int output_map[GX_DEVICE_COLOR_MAX_COMPONENTS];
    int num_known_comp = 0;
    int output_num_comp = target->color_info.num_components;
    int num_sep = pseparations->num_separations++;
    int num_rows_left;

    /*
     * The process color model for the PDF 1.4 compositor device is CMYK plus
     * spot colors.  The target device may have only some of these colorants due
     * to the SeparationOrder device parameter.  Thus we need to determine the
     * mapping between the PDF 1.4 compositor and the target device.  Note:
     * There should not be a spot colorant in the PDF 1.4 device that is not
     * present in the target device.
     */
    /* Check if target processes CMYK colorants. */
    for (comp_num = 0; comp_num < 4; comp_num++) {
        const char * pcomp_name = (const char *)DeviceCMYKComponents[comp_num];

        output_comp_num = dev_proc(target, get_color_comp_index)
                (target, pcomp_name, strlen(pcomp_name), NO_COMP_NAME_TYPE);
        if (output_comp_num >=0 &&
                output_comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS) {
            output_map[num_known_comp] = output_comp_num;
            input_map[num_known_comp++] = comp_num;
        }
    }
    /* Check if target processes our spot colorants. */
    for (comp_num = 0; comp_num < num_sep; comp_num++) {
        output_comp_num = dev_proc(target, get_color_comp_index)
               (target, (const char *)(pseparations->names[comp_num].data),
                pseparations->names[comp_num].size,  NO_COMP_NAME_TYPE);
        if (output_comp_num >= 0 &&
                output_comp_num < GX_DEVICE_COLOR_MAX_COMPONENTS) {
            output_map[num_known_comp] = output_comp_num;
            input_map[num_known_comp++] = comp_num + 4;
        }
    }
    /* See if the target device has a put_image command.  If
       yes then see if it can handle the image data directly.  */
    if (target->procs.put_image != NULL) {
        /* See if the target device can handle the data in its current
           form with the alpha component */
        int alpha_offset = num_comp;
        int tag_offset = has_tags ? num_comp+1 : 0;
        code = dev_proc(target, put_image) (target, buf_ptr, num_comp,
                                            rect.p.x, rect.p.y, width, height,
                                            rowstride, planestride,
                                            num_comp,tag_offset);
        if (code == 0) {
            /* Device could not handle the alpha data.  Go ahead and
               preblend now. Note that if we do this, and we end up in the
               default below, we only need to repack in chunky not blend */
#if RAW_DUMP
            /* Dump before and after the blend to make sure we are doing that ok */
            dump_raw_buffer(height, width, num_comp+1, planestride, rowstride,
                            "pre_final_blend",buf_ptr);
            global_index++;
#endif
            gx_blend_image_buffer(buf_ptr, width, height, rowstride, 
                                  planestride, num_comp, bg);
#if RAW_DUMP
            /* Dump before and after the blend to make sure we are doing that ok */
            dump_raw_buffer(height, width, num_comp, planestride, rowstride,
                            "post_final_blend",buf_ptr);
            global_index++;
            /* clist_band_count++; */
#endif
            /* Try again now */
            alpha_offset = 0;
            code = dev_proc(target, put_image) (target, buf_ptr, num_comp,
                                                rect.p.x, rect.p.y, width, height,
                                                rowstride, planestride,
                                                alpha_offset, tag_offset);
        }
        if (code > 0) {
            /* We processed some or all of the rows.  Continue until we are done */
            num_rows_left = height - code;
            while (num_rows_left > 0) {
                code = dev_proc(target, put_image) (target, buf_ptr, num_comp,
                                                    rect.p.x, rect.p.y+code, width,
                                                    num_rows_left, rowstride,
                                                    planestride,
                                                    alpha_offset, tag_offset);
                if (code < 0)
                    return code;
                num_rows_left = num_rows_left - code;
            }
            return 0;
        }
    }

    /* Clear all output colorants first */
    for (comp_num = 0; comp_num < output_num_comp; comp_num++)
        cv[comp_num] = 0;

    /* Send pixel data to the target device. */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {

            /* composite CMYKA, etc. pixel with over solid background */
            a = buf_ptr[x + planestride * num_comp];

            if ((a + 1) & 0xfe) {
                /* a ^= 0xff; */  /* No inversion here! Bug 689895 */
                for (comp_num = 0; comp_num < num_known_comp; comp_num++) {
                    comp  = buf_ptr[x + planestride * input_map[comp_num]];
                    tmp = ((comp - bg) * a) + 0x80;
                    comp += tmp + (tmp >> 8);
                    cv[output_map[comp_num]] = comp;
                }
            } else if (a == 0) {
                for (comp_num = 0; comp_num < num_known_comp; comp_num++) {
                    cv[output_map[comp_num]] = bg;
                }
            } else {
                for (comp_num = 0; comp_num < num_known_comp; comp_num++) {
                    comp = buf_ptr[x + planestride * input_map[comp_num]];
                    cv[output_map[comp_num]] = (comp << 8) + comp;
                }
            }
            color = dev_proc(target, encode_color)(target, cv);
            code = dev_proc(target, fill_rectangle)(target, x + x0,
                                                            y + y0, 1, 1, color);
            if (code < 0)
                return code;
        }

        buf_ptr += rowstride;
    }
    return code;
}

int
gx_put_blended_image_custom(gx_device *target, byte *buf_ptr,
                      int planestride, int rowstride,
                      int x0, int y0, int width, int height, int num_comp, byte bg)
{
    int code = 0;
    int x, y, tmp, comp_num;
    gx_color_index color;
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value comp;
    byte a;

    /* Send pixel data to the target device. */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {

            /* composite CMYKA, etc. pixel with over solid background */
            a = buf_ptr[x + planestride * num_comp];

            if ((a + 1) & 0xfe) {
                a ^= 0xff;
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    comp  = buf_ptr[x + planestride * comp_num];
                    tmp = ((bg - comp) * a) + 0x80;
                    comp += tmp + (tmp >> 8);
                    cv[comp_num] = comp;
                }
            } else if (a == 0) {
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    cv[comp_num] = bg;
                }
            } else {
                for (comp_num = 0; comp_num < num_comp; comp_num++) {
                    comp = buf_ptr[x + planestride * comp_num];
                    cv[comp_num] = (comp << 8) + comp;
                }
            }
            color = dev_proc(target, encode_color)(target, cv);
            code = dev_proc(target, fill_rectangle)(target, x + x0,
                                                            y + y0, 1, 1, color);
            if (code < 0)
                return code;
        }

        buf_ptr += rowstride;
    }
    return code;
}
