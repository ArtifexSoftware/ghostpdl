/* Copyright (C) 2001-2024 Artifex Software, Inc.
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

/* PDF 1.4 blending functions */

#include "memory_.h"
#include "gx.h"
#include "gp.h"
#include "gstparam.h"
#include "gsrect.h"
#include "gxblend.h"
#include "gxdcconv.h"
#include "gxdevcli.h"
#include "gxgstate.h"
#include "gdevdevn.h"
#include "gdevp14.h"
#include "gxdcconv.h"
#include "gsicc_cache.h"
#include "gxdevsop.h"

#ifdef DUMP_TO_PNG
#include "png_.h"
#endif

/* A case where we have RGB + spots.  This is actually an RGB color and we
 * should zero out the spot colorants */
void
pdf14_unpack_rgb_mix(int num_comp, gx_color_index color,
                     pdf14_device * p14dev, byte * out)
{
    int i;

    memset(out, 0, num_comp);
    for (i = 2; i >= 0; i--) {
        out[i] = (byte)(color & 0xff);
        color >>= 8;
    }
}

void
pdf14_unpack16_rgb_mix(int num_comp, gx_color_index color,
                       pdf14_device * p14dev, uint16_t * out)
{
    int i;

    memset(out, 0, num_comp);
    for (i = 2; i >= 0; i--) {
        out[i] = (uint16_t)color;
        color >>= 16;
    }
}

/* A case where we have Gray + spots.  This is actually a Gray color and we
* should zero out the spot colorants */
void
pdf14_unpack_gray_mix(int num_comp, gx_color_index color,
                      pdf14_device * p14dev, byte * out)
{
    memset(out, 0, num_comp);
    out[0] = (byte)(color & 0xff);
}

void
pdf14_unpack16_gray_mix(int num_comp, gx_color_index color,
                        pdf14_device * p14dev, uint16_t * out)
{
    memset(out, 0, num_comp);
    out[0] = (uint16_t)color;
}

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

void
pdf14_unpack16_additive(int num_comp, gx_color_index color,
                        pdf14_device * p14dev, uint16_t * out)
{
    int i;

    for (i = num_comp - 1; i >= 0; i--) {
        out[i] = (uint16_t)color;
        color >>= 16;
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

void
pdf14_unpack16_subtractive(int num_comp, gx_color_index color,
                           pdf14_device * p14dev, uint16_t * out)
{
    int i;

    for (i = num_comp - 1; i >= 0; i--) {
        out[i] = (uint16_t)~color;
        color >>= 16;
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

void
pdf14_unpack16_custom(int num_comp, gx_color_index color,
                      pdf14_device * p14dev, uint16_t * out)
{
    int i;
    gx_device * tdev = p14dev->target;
    gx_color_value cm_values[GX_DEVICE_COLOR_MAX_COMPONENTS];

    dev_proc(tdev, decode_color)(tdev, color, cm_values);
    for (i = 0; i < num_comp; i++)
        out[i] = ~cm_values[i];
}

#if RAW_DUMP
extern unsigned int global_index;
#endif

static void
copy_plane_part(byte *des_ptr, int des_rowstride, byte *src_ptr, int src_rowstride,
                int width, int height, bool deep)
{
    int y;

    width <<= deep;

    if (width == des_rowstride && width == src_rowstride) {
        width *= height;
        height = 1;
    }

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
    des_buf += des_info->planestride * (size_t)((des_info->has_shape ? 1 : 0) +
                                        (des_info->has_alpha_g ? 1 : 0));
    src_buf += src_info->planestride * (size_t)((src_info->has_shape ? 1 : 0) +
                                        (src_info->has_alpha_g ? 1 : 0));
    /* tags plane does copy */
    if (des_info->has_tags) {
        if (src_info->has_tags) {
            copy_plane_part(des_buf, des_info->rowstride, src_buf,
                            src_info->rowstride, width, height, src_info->deep);
        }
    }
}

int
pdf14_preserve_backdrop_cm(pdf14_buf *buf, cmm_profile_t *group_profile,
                           pdf14_buf *tos, cmm_profile_t *tos_profile,
                           gs_memory_t *memory, gs_gstate *pgs, gx_device *dev,
                           bool knockout_buff)
{
    /* Make copy of backdrop, but convert to new group's colorspace */
    int x0 = max(buf->rect.p.x, tos->rect.p.x);
    int x1 = min(buf->rect.q.x, tos->rect.q.x);
    int y0 = max(buf->rect.p.y, tos->rect.p.y);
    int y1 = min(buf->rect.q.y, tos->rect.q.y);
    bool deep = buf->deep;
    int code;

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
        rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
        rendering_params.cmm = gsCMM_DEFAULT;
        /* Request the ICC link for the transform that we will need to use */
        icc_link = gsicc_get_link_profile(pgs, dev, tos_profile, group_profile,
                                          &rendering_params, memory, false);
        if (icc_link == NULL)
            return gs_throw(gs_error_unknownerror, "ICC link failed.  Trans backdrop");

        if (icc_link->is_identity) {
            pdf14_preserve_backdrop(buf, tos, knockout_buff
#if RAW_DUMP
                                    , dev->memory
#endif
                                    );
            gsicc_release_link(icc_link);
            return 0;
        } else {
            if (knockout_buff) {
                buf_plane = buf->backdrop + ((x0 - buf->rect.p.x)<<deep) +
                        (y0 - buf->rect.p.y) * (size_t)buf->rowstride;
                tos_plane = tos->backdrop + ((x0 - tos->rect.p.x)<<deep) +
                        (y0 - tos->rect.p.y) * (size_t)tos->rowstride;
                memset(buf->backdrop, 0, buf->n_chan * ((size_t)buf->planestride)<<deep);
            } else {
                buf_plane = buf->data + ((x0 - buf->rect.p.x)<<deep) +
                        (y0 - buf->rect.p.y) * (size_t)buf->rowstride;
                tos_plane = tos->data + ((x0 - tos->rect.p.x)<<deep) +
                        (y0 - tos->rect.p.y) * (size_t)tos->rowstride;
                /* First clear out everything. There are cases where the incoming buf
                   has a region outside the existing tos group.  Need to check if this
                   is getting clipped in which case we need to fix the allocation of
                   the buffer to be smaller */
                memset(buf->data, 0, buf->n_planes * ((size_t)buf->planestride)<<deep);
            }
            /* Set up the buffer descriptors. */
            gsicc_init_buffer(&input_buff_desc, tos_profile->num_comps, 1<<deep, false,
                              false, true, tos->planestride, tos->rowstride, height,
                              width);
            gsicc_init_buffer(&output_buff_desc, group_profile->num_comps, 1<<deep, false,
                              false, true, buf->planestride, buf->rowstride, height,
                              width);
            /* Transform the data.  */
            code = (icc_link->procs.map_buffer)(dev, icc_link, &input_buff_desc,
                                         &output_buff_desc, tos_plane, buf_plane);
            gsicc_release_link(icc_link);
            if (code < 0)
                return gs_throw(gs_error_unknownerror, "ICC transform failed.  Trans backdrop");
        }
        /* Copy the alpha data */
        buf_plane += (buf->planestride) * (size_t)(buf->n_chan - 1);
        tos_plane += (tos->planestride) * (size_t)(tos->n_chan - 1);
        copy_plane_part(buf_plane, buf->rowstride, tos_plane, tos->rowstride, width,
                        height, deep);
        buf_plane += buf->planestride;
        tos_plane += tos->planestride;

        if (!knockout_buff)
            copy_extra_planes(buf_plane, buf, tos_plane, tos, width, height);
    }
#if RAW_DUMP
    if (x0 < x1 && y0 < y1) {
        byte *buf_plane = buf->data + ((x0 - buf->rect.p.x)<<deep) +
            (y0 - buf->rect.p.y) * (size_t)buf->rowstride;
        dump_raw_buffer(dev->memory, y1 - y0, x1 - x0, buf->n_planes, buf->planestride,
                        buf->rowstride, "BackDropInit_CM", buf_plane, deep);
        global_index++;
    }
#endif
    return 0;
}

void
pdf14_preserve_backdrop(pdf14_buf *buf, pdf14_buf *tos, bool from_backdrop
#if RAW_DUMP
                        , const gs_memory_t *mem
#endif
                        )
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
        int i, n_planes;
        bool deep = buf->deep;

        buf_plane = buf->data;
        n_planes = buf->n_planes;
        if (from_backdrop) {
            tos_plane = tos->backdrop;
        } else {
            tos_plane = tos->data;
        }

        /* First clear out everything. There are cases where the incoming buf
           has a region outside the existing tos group.  Need to check if this
           is getting clipped in which case we need to fix the allocation of
           the buffer to be smaller */
        if (x0 > buf->rect.p.x || x1 < buf->rect.q.x ||
            y0 > buf->rect.p.y || y1 < buf->rect.q.y) {
            /* FIXME: There is potential for more optimisation here,
             * but I don't know how often we hit this case. */
            memset(buf_plane, 0, n_planes * (size_t)buf->planestride);
        } else if (n_planes > tos->n_chan) {
            /* The next planes are alpha_g, shape, tags. We need to clear
             * alpha_g and shape, but don't need to clear the tag plane
             * if it would be copied below (and if it exists). */
            int tag_plane_num = tos->n_chan + !!buf->has_shape + !!buf->has_alpha_g;
            if (!from_backdrop && n_planes > tag_plane_num)
                n_planes = tag_plane_num;
            if (n_planes > tos->n_chan)
                memset(buf->data + tos->n_chan * (size_t)buf->planestride, 0,
                       (n_planes - tos->n_chan) * (size_t)buf->planestride);
        }
        buf_plane += (y0 - buf->rect.p.y) * (size_t)buf->rowstride +
                     ((x0 - buf->rect.p.x)<<deep);
        tos_plane += (y0 - tos->rect.p.y) * (size_t)tos->rowstride +
                     ((x0 - tos->rect.p.x)<<deep);
        /* Color and alpha plane */
        for (i = 0; i < tos->n_chan; i++) {
            copy_plane_part(buf_plane, buf->rowstride, tos_plane, tos->rowstride,
                            width, height, buf->deep);
            buf_plane += buf->planestride;
            tos_plane += tos->planestride;
        }
        if (!from_backdrop)
            copy_extra_planes(buf_plane, buf, tos_plane, tos, width, height);
    }
#if RAW_DUMP
    if (x0 < x1 && y0 < y1) {
        byte *buf_plane = (from_backdrop ? buf->backdrop : buf->data);
        if (buf_plane != NULL) {
            buf_plane += ((x0 - buf->rect.p.x) << buf->deep) +
                (y0 - buf->rect.p.y) * (size_t)buf->rowstride;
            dump_raw_buffer(mem, y1 - y0, x1 - x0, buf->n_planes, buf->planestride,
                buf->rowstride, "BackDropInit", buf_plane, buf->deep);
            global_index++;
        }
    }
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

gx_color_index
pdf14_encode_color16(gx_device *dev, const gx_color_value	colors[])
{
    gx_color_index color = 0;
    uchar i;
    uchar ncomp = dev->color_info.num_components;
    COLROUND_VARS;

    COLROUND_SETUP(16);
    for (i = 0; i < ncomp; i++) {
        color <<= 16;
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
    uchar ncomp = dev->color_info.num_components - 1;
    COLROUND_VARS;

    COLROUND_SETUP(8);
    /* Add in the tag information */
    color = dev->graphics_type_tag & 255;
    for (i = 0; i < ncomp; i++) {
        color <<= 8;
        color |= COLROUND_ROUND(colors[i]);
    }
    return (color == gx_no_color_index ? color ^ 1 : color);
}

gx_color_index
pdf14_encode_color16_tag(gx_device *dev, const gx_color_value colors[])
{
    gx_color_index color;
    uchar i;
    uchar ncomp = dev->color_info.num_components - 1;
    COLROUND_VARS;

    COLROUND_SETUP(16);
    /* Add in the tag information */
    color = dev->graphics_type_tag & 65535;
    for (i = 0; i < ncomp; i++) {
        color <<= 16;
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

int
pdf14_decode_color16(gx_device * dev, gx_color_index color, gx_color_value * out)
{
    uchar i;
    uchar ncomp = dev->color_info.num_components;

    for (i = 0; i < ncomp; i++) {
        out[ncomp - i - 1] = (gx_color_value) (color & 0xffff);
        color >>= 16;
    }
    return 0;
}

void
pdf14_gray_cs_to_cmyk_cm(const gx_device * dev, frac gray, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = out[1] = out[2] = frac_0;
    out[3] = frac_1 - gray;
    for (--num_comp; num_comp > 3; num_comp--)
        out[num_comp] = 0;
}

/* These three must handle rgb + spot */
void
pdf14_gray_cs_to_rgbspot_cm(const gx_device * dev, frac gray, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = out[1] = out[2] = gray;
    for (--num_comp; num_comp > 2; num_comp--)
        out[num_comp] = 0;
}

void
pdf14_rgb_cs_to_rgbspot_cm(const gx_device * dev, const gs_gstate *pgs,
    frac r, frac g, frac b, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = r;
    out[1] = g;
    out[2] = b;
    for (--num_comp; num_comp > 2; num_comp--)
        out[num_comp] = 0;
}

void
pdf14_cmyk_cs_to_rgbspot_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    color_cmyk_to_rgb(c, m, y, k, NULL, out, dev->memory);
    for (--num_comp; num_comp > 2; num_comp--)
        out[num_comp] = 0;
}

/* These three must handle gray + spot */
void
pdf14_gray_cs_to_grayspot_cm(const gx_device * dev, frac gray, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = gray;
    for (--num_comp; num_comp > 0; num_comp--)
        out[num_comp] = 0;
}

void
pdf14_rgb_cs_to_grayspot_cm(const gx_device * dev, const gs_gstate *pgs,
    frac r, frac g, frac b, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = (r + g + b) / 3;
    for (--num_comp; num_comp > 0; num_comp--)
        out[num_comp] = 0;
}

void
pdf14_cmyk_cs_to_grayspot_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    out[0] = color_cmyk_to_gray(c, m, y, k, NULL);
    for (--num_comp; num_comp > 0; num_comp--)
        out[num_comp] = 0;
}

/*
 * Default map from DeviceRGB color space to DeviceCMYK color
 * model. Since this mapping is defined by the PostScript language
 * it is unlikely that any device with a DeviceCMYK color model
 * would define this mapping on its own.
 *
 * If the gs_gstate is not available, map as though the black
 * generation and undercolor removal functions are identity
 * transformations. This mode is used primarily to support the
 * raster operation (rop) feature of PCL, which requires that
 * the raster operation be performed in an RGB color space.
 * Note that default black generation and undercolor removal
 * functions in PostScript need NOT be identity transformations:
 * often they are { pop 0 }.
 */
void
pdf14_rgb_cs_to_cmyk_cm(const gx_device * dev, const gs_gstate *pgs,
                           frac r, frac g, frac b, frac out[])
{
    uchar num_comp = dev->color_info.num_components;

    if (pgs != 0)
        color_rgb_to_cmyk(r, g, b, pgs, out, dev->memory);
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
pdf14_cmyk_cs_to_cmyk_cm(const gx_device * dev, frac c, frac m, frac y, frac k, frac out[])
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
    size_t rowstride = pbuf->rowstride, planestride = pbuf->planestride;
    int rowbytes = width << 2;
    gs_int_rect rect = buf->rect;
    int x1 = min(pdev->width, rect.q.x);
    int y1 = min(pdev->height, rect.q.y);
    int width = x1 - rect.p.x;
    int height = y1 - rect.p.y;
    byte *buf_ptr = buf->data + rect.p.y * (size_t)buf->rowstride + rect.p.x;
    byte *row = gs_malloc(mem, rowbytes, 1, "png raster buffer");
    png_struct *png_ptr =
    png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info *info_ptr =
    png_create_info_struct(png_ptr);
    const char *software_key = "Software";
    char software_text[256];
    png_text text_png;
    gp_file *file;
    int code;
    int y;

    if (buf->data == NULL)
        return 0;

    file = gp_fopen (mem, "c:\\temp\\tmp.png", "wb");

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
    gs_snprintf(software_text, sizeof(software_text), "%s %d.%02d", gs_product,
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
            row[(x << 2) + 2] = buf_ptr[x + planestride * (size_t)2];
            row[(x << 2) + 3] = buf_ptr[x + planestride * (size_t)3];
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
gx_build_blended_image_row(const byte *gs_restrict buf_ptr, int planestride,
                           int width, int num_comp, uint16_t bg, byte *gs_restrict linebuf)
{
    size_t inc = planestride * (size_t)num_comp;

    buf_ptr += inc - 1;
    for (; width > 0; width--) {
        /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
        byte a = *++buf_ptr;
        int i = num_comp;

        if (a == 0) {
            do {
                *linebuf++ = bg;
            } while (--i);
        } else {
            buf_ptr -= inc;
            if (a == 0xff) {
                do {
                    *linebuf++ = *buf_ptr;
                    buf_ptr += planestride;
                } while (--i);
            } else {
                a ^= 0xff;
                do {
                    byte comp = *buf_ptr;
                    int tmp = ((bg - comp) * a) + 0x80;
                    buf_ptr += planestride;
                    comp += (tmp + (tmp >> 8)) >> 8;
                    *linebuf++ = comp;
                } while (--i);
            }
        }
    }
}

void
gx_build_blended_image_row16(const byte *gs_restrict buf_ptr_, int planestride,
                             int width, int num_comp, uint16_t bg, byte *gs_restrict linebuf)
{
    const uint16_t *gs_restrict buf_ptr = (const uint16_t *)(const void *)buf_ptr_;
    size_t inc;

    /* Note that we read in in native endian and blend,
     * then store out in big endian. */
    planestride >>= 1; /* Array indexing, not byte indexing */
    inc = planestride * (size_t)num_comp;
    buf_ptr += inc - 1;
    for (; width > 0; width--) {
        /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
        uint16_t a = *++buf_ptr;
        int i = num_comp;

        if (a == 0) {
            do {
                *linebuf++ = bg>>8;
                *linebuf++ = bg;
            } while (--i);
        } else {
            buf_ptr -= inc;
            if (a == 0xffff) {
                do {
                    uint16_t comp = *buf_ptr;
                    *linebuf++ = comp>>8;
                    *linebuf++ = comp;
                    buf_ptr += planestride;
                } while (--i);
            } else {
                a ^= 0xffff;
                a += a>>15;
                do {
                    uint32_t comp = *buf_ptr;
                    comp += (((bg - comp) * a) + 0x8000)>>16;
                    /* Errors in bit 16 and above will be ignored */
                    buf_ptr += planestride;
                    *linebuf++ = comp>>8;
                    *linebuf++ = comp;
                } while (--i);
            }
        }
    }
}

void
gx_blend_image_buffer(byte *buf_ptr, int width, int height, int rowstride,
                      int planestride, int num_comp, int additive)
{
    int x, y;
    int position;
    byte comp, a;
    int tmp, comp_num;

    if (additive) {
        if (num_comp > 3) {
            for (y = 0; y < height; y++) {
                position = y * rowstride;
                for (x = 0; x < width; x++) {
                    /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
                    a = buf_ptr[position + planestride * (size_t)num_comp];
                    if ((a + 1) & 0xfe) {
                        int b = 0xFF;
                        a ^= 0xff;
                        for (comp_num = 0; comp_num < num_comp; comp_num++) {
                            if (comp_num == 3)
                                b = 0;
                            comp = buf_ptr[position + planestride * (size_t)comp_num];
                            tmp = ((b - comp) * a) + 0x80;
                            comp += (tmp + (tmp >> 8)) >> 8;
                            buf_ptr[position + planestride * (size_t)comp_num] = comp;
                        }
                    } else if (a == 0) {
                        for (comp_num = 0; comp_num < 3; comp_num++)
                            buf_ptr[position + planestride * (size_t)comp_num] = 0xFF;
                        for (; comp_num < num_comp; comp_num++)
                            buf_ptr[position + planestride * (size_t)comp_num] = 0;
                    }
                    position += 1;
                }
            }
        }
        else {
            for (y = 0; y < height; y++) {
                position = y * rowstride;
                for (x = 0; x < width; x++) {
                    /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
                    a = buf_ptr[position + planestride * (size_t)num_comp];
                    if ((a + 1) & 0xfe) {
                        a ^= 0xff;
                        for (comp_num = 0; comp_num < num_comp; comp_num++) {
                            comp = buf_ptr[position + planestride * (size_t)comp_num];
                            tmp = ((0xFF - comp) * a) + 0x80;
                            comp += (tmp + (tmp >> 8)) >> 8;
                            buf_ptr[position + planestride * (size_t)comp_num] = comp;
                        }
                    } else if (a == 0) {
                        for (comp_num = 0; comp_num < num_comp; comp_num++)
                            buf_ptr[position + planestride * (size_t)comp_num] = 0xFF;
                    }
                    position += 1;
                }
            }
        }
    } else {
        for (y = 0; y < height; y++) {
            position = y * rowstride;
            for (x = 0; x < width; x++) {
                /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
                a = buf_ptr[position + planestride * (size_t)num_comp];
                if ((a + 1) & 0xfe) {
                    a ^= 0xff;
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        comp = buf_ptr[position + planestride * (size_t)comp_num];
                        tmp = ((- comp) * a) + 0x80;
                        comp += (tmp + (tmp >> 8)) >> 8;
                        buf_ptr[position + planestride * (size_t)comp_num] = comp;
                    }
                } else if (a == 0) {
                    for (comp_num = 0; comp_num < num_comp; comp_num++)
                        buf_ptr[position + planestride * (size_t)comp_num] = 0;
                }
                position += 1;
            }
        }
    }
}

void
gx_blend_image_buffer16(byte *buf_ptr_, int width, int height, int rowstride,
                        int planestride, int num_comp, int additive, bool keep_native)
{
    uint16_t *buf_ptr = (uint16_t *)(void *)buf_ptr_;
    int x, y;
    int position;
    int comp, a;
    int tmp, comp_num;

    /* planestride and rowstride are in bytes, and we want them in shorts */
    planestride >>= 1;
    rowstride >>= 1;

    if (additive)
    {
        if (num_comp > 3) {
            /* Note that the input here is native endian, and the output must be in big endian! */
            for (y = 0; y < height; y++) {
                position = y * rowstride;
                for (x = 0; x < width; x++) {
                    /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
                    a = buf_ptr[position + planestride * (size_t)num_comp];
                    if (a == 0) {
                        for (comp_num = 0; comp_num < 3; comp_num++)
                            buf_ptr[position + planestride * (size_t)comp_num] = 65535;
                        for (; comp_num < num_comp; comp_num++)
                            buf_ptr[position + planestride * (size_t)comp_num] = 0;
                    } else if (a == 0xffff) {
#if ARCH_IS_BIG_ENDIAN
#else
                        if (!keep_native) {
                            for (comp_num = 0; comp_num < num_comp; comp_num++) {
                                comp = buf_ptr[position + planestride * (size_t)comp_num];
                                ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[0] = comp >> 8;
                                ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[1] = comp;
                            }
                        }
#endif
                    } else {
                        int b = 65535;
                        a ^= 0xffff;
                        a += a >> 15; /* a is now 0 to 0x10000 */
                        a >>= 1; /* We can only use 15 bits as bg-comp has a sign bit we can't lose */
                        for (comp_num = 0; comp_num < num_comp; comp_num++) {
                            if (comp_num == 3)
                                b = 0;
                            comp = buf_ptr[position + planestride * (size_t)comp_num];
                            tmp = ((b - comp) * a) + 0x4000;
                            comp += (tmp >> 15); /* Errors in bit 16 upwards will be ignored */
                            /* Store as big endian */
                            ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[0] = comp >> 8;
                            ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[1] = comp;
                        }
                    }
                    position += 1;
                }
            }
        }
        else {
            /* Note that the input here is native endian, and the output must be in big endian! */
            for (y = 0; y < height; y++) {
                position = y * rowstride;
                for (x = 0; x < width; x++) {
                    /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
                    a = buf_ptr[position + planestride * (size_t)num_comp];
                    if (a == 0) {
                        for (comp_num = 0; comp_num < num_comp; comp_num++) {
                            buf_ptr[position + planestride * (size_t)comp_num] = 65535;
                        }
                    } else if (a == 0xffff) {
#if ARCH_IS_BIG_ENDIAN
#else
                        if (!keep_native) {
                            for (comp_num = 0; comp_num < num_comp; comp_num++) {
                                comp = buf_ptr[position + planestride * (size_t)comp_num];
                                ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[0] = comp >> 8;
                                ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[1] = comp;
                            }
                        }
#endif
                    } else {
                        a ^= 0xffff;
                        a += a >> 15; /* a is now 0 to 0x10000 */
                        a >>= 1; /* We can only use 15 bits as bg-comp has a sign bit we can't lose */
                        for (comp_num = 0; comp_num < num_comp; comp_num++) {
                            comp = buf_ptr[position + planestride * (size_t)comp_num];
                            tmp = ((65535 - comp) * a) + 0x4000;
                            comp += (tmp >> 15); /* Errors in bit 16 upwards will be ignored */
                            /* Store as big endian */
                            ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[0] = comp >> 8;
                            ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[1] = comp;
                        }
                    }
                    position += 1;
                }
            }
        }
    } else {
        /* Note that the input here is native endian, and the output must be in big endian! */
        for (y = 0; y < height; y++) {
            position = y * rowstride;
            for (x = 0; x < width; x++) {
                /* composite RGBA (or CMYKA, etc.) pixel with over solid background */
                a = buf_ptr[position + planestride * (size_t)num_comp];
                if (a == 0) {
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        buf_ptr[position + planestride * (size_t)comp_num] = 0;
                    }
                } else if (a == 0xffff) {
#if ARCH_IS_BIG_ENDIAN
#else
                    if (!keep_native) {
                        for (comp_num = 0; comp_num < num_comp; comp_num++) {
                            comp = buf_ptr[position + planestride * (size_t)comp_num];
                            ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[0] = comp >> 8;
                            ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[1] = comp;
                        }
                    }
#endif
                } else {
                    a ^= 0xffff;
                    a += a >> 15; /* a is now 0 to 0x10000 */
                    a >>= 1; /* We can only use 15 bits as -comp has a sign bit we can't lose */
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        comp = buf_ptr[position + planestride * (size_t)comp_num];
                        tmp = ((- comp) * a) + 0x4000;
                        comp += (tmp >> 15); /* Errors in bit 16 upwards will be ignored */
                        /* Store as big endian */
                        ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[0] = comp >> 8;
                        ((byte *)&buf_ptr[position + planestride * (size_t)comp_num])[1] = comp;
                    }
                }
                position += 1;
            }
        }
    }
}

int
gx_put_blended_image_custom(gx_device *target, byte *buf_ptr_,
                      int planestride, int rowstride,
                      int x0, int y0, int width, int height,
                      int num_comp, uint16_t bg, bool deep)
{
    int code = 0;
    int x, y, tmp, comp_num;
    gx_color_index color;
    gx_color_value cv[GX_DEVICE_COLOR_MAX_COMPONENTS];
    gx_color_value comp;
    uint16_t *buf_ptr = (uint16_t *)(void *)buf_ptr_;

    /* Send pixel data to the target device. */
    if (deep) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {

                /* composite CMYKA, etc. pixel with over solid background */
#define GET16(v) (*((uint16_t *)(void *)&(v)))
                uint16_t a = GET16(buf_ptr[x + planestride * (size_t)num_comp]);

                if (a == 0) {
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        cv[comp_num] = bg;
                    }
                } else if (a == 0xffff) {
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        comp = buf_ptr[x + planestride * (size_t)comp_num];
                        cv[comp_num] = comp;
                    }
                } else {
                    a ^= 0xffff;
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        comp  = GET16(buf_ptr[x + planestride * (size_t)comp_num]);
                        tmp = ((bg - comp) * a) + 0x8000;
                        cv[comp_num] = comp + ((tmp + (tmp>>16))>>16);
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
    } else {
        bg >>= 8;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {

                /* composite CMYKA, etc. pixel with over solid background */
                byte a = buf_ptr[x + planestride * (size_t)num_comp];

                if ((a + 1) & 0xfe) {
                    a ^= 0xff;
                    for (comp_num = 0; comp_num < num_comp; comp_num++) {
                        comp  = buf_ptr[x + planestride * (size_t)comp_num];
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
                        comp = buf_ptr[x + planestride * (size_t)comp_num];
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
    }
    return code;
}
