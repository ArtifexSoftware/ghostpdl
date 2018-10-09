/* Copyright (C) 2001-2018 Artifex Software, Inc.
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


/* Chameleon devices to allow for changing bit depths/colors at runtime. */

#include "gdevprn.h"
#include "gsparam.h"
#include "gscrd.h"
#include "gscrdp.h"
#include "gxlum.h"
#include "gxdcconv.h"
#include "gdevdcrd.h"
#include "gxdownscale.h"

/* Define the device parameters. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* The device descriptor */
static dev_proc_encode_color(chameleon_mono_encode_color);
static dev_proc_encode_color(chameleon_rgb_encode_color);
static dev_proc_encode_color(chameleon_cmyk_encode_color);
static dev_proc_decode_color(chameleon_mono_decode_color);
static dev_proc_decode_color(chameleon_rgb_decode_color);
static dev_proc_decode_color(chameleon_cmyk_decode_color);
static dev_proc_get_params(chameleon_get_params);
static dev_proc_put_params(chameleon_put_params);
static dev_proc_print_page(chameleon_print_page);
static dev_proc_put_image(chameleon_put_image);

struct gx_device_chameleon_s {
    gx_device_common;
    gx_prn_device_common;
    int num_components;
    int bpc;
    int dst_num_components;
    int dst_bpc;
    int output_as_pxm;
    gx_downscaler_params downscale;
};
typedef struct gx_device_chameleon_s gx_device_chameleon;

static const gx_device_procs chameleon_procs =
{	gdev_prn_open,
        gx_default_get_initial_matrix,
        NULL,	/* sync_output */
        /* Since the print_page doesn't alter the device, this device can print in the background */
        gdev_prn_bg_output_page,
        gdev_prn_close,
        chameleon_rgb_encode_color,	/* map_rgb_color */
        chameleon_rgb_decode_color,	/* map_color_rgb */
        NULL,	/* fill_rectangle */
        NULL,	/* tile_rectangle */
        NULL,	/* copy_mono */
        NULL,	/* copy_color */
        NULL,	/* draw_line */
        NULL,	/* get_bits */
        chameleon_get_params,
        chameleon_put_params,
        chameleon_rgb_encode_color,	/* map_cmyk_color */
        NULL,	/* get_xfont_procs */
        NULL,	/* get_xfont_device */
        NULL,	/* map_rgb_alpha_color */
        gx_page_device_get_page_device,	/* get_page_device */
        NULL,	/* get_alpha_bits */
        NULL,	/* copy_alpha */
        NULL,	/* get_band */
        NULL,	/* copy_rop */
        NULL,	/* fill_path */
        NULL,	/* stroke_path */
        NULL,	/* fill_mask */
        NULL,	/* fill_trapezoid */
        NULL,	/* fill_parallelogram */
        NULL,	/* fill_triangle */
        NULL,	/* draw_thin_line */
        NULL,	/* begin_image */
        NULL,	/* image_data */
        NULL,	/* end_image */
        NULL,	/* strip_tile_rectangle */
        NULL,	/* strip_copy_rop */
        NULL,	/* get_clipping_box */
        NULL,	/* begin_typed_image */
        NULL,	/* get_bits_rectangle */
        NULL,	/* map_color_rgb_alpha */
        NULL,	/* create_compositor */
        NULL,	/* get_hardware_params */
        NULL,	/* text_begin */
        NULL,	/* finish_copydevice */
        NULL,	/* begin_transparency_group */
        NULL,	/* end_transparency_group */
        NULL,	/* begin_transparency_mask */
        NULL,	/* end_transparency_mask */
        NULL,	/* discard_transparency_layer */
        NULL,	/* get_color_mapping_procs */
        NULL,	/* get_color_comp_index */
        chameleon_rgb_encode_color,/* encode_color */
        chameleon_rgb_decode_color /* decode_color */
};

const gx_device_chameleon gs_chameleon_device =
{prn_device_body(gx_device_chameleon, chameleon_procs, "chameleon",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, /* 3 colors (by default) */
		 24, /* depth */
                 255, /* max grey */
                 255, /* max color */
                 256, /* dither greys */
                 256, /* dither colors */
                 chameleon_print_page),
        3,
        8,
        4,
        1,
        0,
        GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* Map gray to color. */
/* Note that 1-bit monochrome is a special case. */
static gx_color_index
chameleon_mono_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_value gray = cv[0];

    return (bpc == 1 ? gx_max_color_value - gray : gray) >> drop;
}

static gx_color_index
chameleon_rgb_encode_color(gx_device * dev, const gx_color_value cv[])
{
    if (dev->color_info.depth == 24)
        return gx_color_value_to_byte(cv[2]) +
            ((uint) gx_color_value_to_byte(cv[1]) << 8) +
            ((ulong) gx_color_value_to_byte(cv[0]) << 16);
    else {
        COLROUND_VARS;
        /* The following needs special handling to avoid bpc=5 when depth=16 */
        int bpc = dev->color_info.depth == 16 ? 4 : dev->color_info.depth / 3;
        COLROUND_SETUP(bpc);

        return (((COLROUND_ROUND(cv[0]) << bpc) +
                 COLROUND_ROUND(cv[1])) << bpc) +
               COLROUND_ROUND(cv[2]);
    }
}

/* Map CMYK to color. */
static gx_color_index
chameleon_cmyk_encode_color(gx_device * dev, const gx_color_value cv[])
{
    int bpc = dev->color_info.depth / 4;
    int drop = sizeof(gx_color_value) * 8 - bpc;
    gx_color_index color =
    (((((((gx_color_index) cv[0] >> drop) << bpc) +
        (cv[1] >> drop)) << bpc) +
      (cv[2] >> drop)) << bpc) +
    (cv[3] >> drop);

    return (color == gx_no_color_index ? color ^ 1 : color);
}

/* Map color to RGB.  This has 3 separate cases, but since it is rarely */
/* used, we do a case test rather than providing 3 separate routines. */
static int
chameleon_mono_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[4])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    cv[0] = cv[1] = cv[2] =
        (depth == 1 ? (color ? 0 : gx_max_color_value) :
        cvalue(color));
    return 0;
#undef cvalue
}

static int
chameleon_rgb_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[4])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    gx_color_index cshift = color;

    cv[2] = cvalue(cshift & mask);
    cshift >>= bpc;
    cv[1] = cvalue(cshift & mask);
    cv[0] = cvalue(cshift >> bpc);
    return 0;
#undef cvalue
}

static int
chameleon_cmyk_decode_color(gx_device * dev, gx_color_index color, gx_color_value cv[4])
{
    int depth = dev->color_info.depth;
    int ncomp = dev->color_info.num_components;
    int bpc = depth / ncomp;
    uint mask = (1 << bpc) - 1;

#define cvalue(c) ((gx_color_value)((ulong)(c) * gx_max_color_value / mask))

    gx_color_index cshift = color;
    uint c, m, y, k;

    k = cshift & mask;
    cshift >>= bpc;
    y = cshift & mask;
    cshift >>= bpc;
    m = cshift & mask;
    c = cshift >> bpc;
    /* We use our improved conversion rule.... */
    cv[0] = cvalue((mask - c) * (mask - k) / mask);
    cv[1] = cvalue((mask - m) * (mask - k) / mask);
    cv[2] = cvalue((mask - y) * (mask - k) / mask);
    return 0;
#undef cvalue
}

/* Get parameters.  We provide a default CRD. */
static int
chameleon_get_params(gx_device * pdev, gs_param_list * plist)
{
    int code, ecode;
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;

    ecode = gdev_prn_get_params(pdev, plist);
    code = sample_device_crd_get_params(pdev, plist, "CRDDefault");
    if (code < 0)
            ecode = code;

    if ((code = param_write_int(plist, "BitDepth", &pcdev->bpc)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "Components", &pcdev->num_components)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "DstBitDepth", &pcdev->dst_bpc)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "DstComponents", &pcdev->dst_num_components)) < 0)
        ecode = code;
    if ((code = gx_downscaler_write_params(plist, &pcdev->downscale, 0)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "OutputAsPXM", &pcdev->output_as_pxm)) < 0)
        ecode = code;

    return ecode;
}

/* Set parameters.  We allow setting the number of bits per component, and number of components. */
static int
chameleon_put_params(gx_device * pdev, gs_param_list * plist)
{
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;
    gx_device_color_info save_info;
    int num_comps = pcdev->num_components;
    int bpc = pcdev->bpc;
    int pxm = pcdev->output_as_pxm;
    int dst_num_comps = pcdev->dst_num_components;
    int dst_bpc = pcdev->dst_bpc;
    int ecode;
    int code;
    const char *vname;

    ecode = gx_downscaler_read_params(plist, &pcdev->downscale, 0);

    if ((code = param_read_int(plist, (vname = "BitDepth"), &bpc)) != 1) {
        if (code < 0)
            ecode = code;
        else
            switch (bpc) {
                case 1: case 2: case 4: case 8: break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    if ((code = param_read_int(plist, (vname = "DstBitDepth"), &dst_bpc)) != 1) {
        if (code < 0)
            ecode = code;
        else
            switch (bpc) {
                case 1: case 2: case 4: case 8: break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    if ((code = param_read_int(plist, (vname = "Components"), &num_comps)) != 1) {
        if (code < 0)
            ecode = code;
        else
            switch (num_comps) {
                case 1: case 3: case 4: break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    if ((code = param_read_int(plist, (vname = "DstComponents"), &dst_num_comps)) != 1) {
        if (code < 0)
            ecode = code;
        else
            switch (num_comps) {
                case 1: case 3: case 4: break;
                default:
                    param_signal_error(plist, vname,
                                       ecode = gs_error_rangecheck);
            }
    }

    if ((code = param_read_int(plist, (vname = "OutputAsPXM"), &pxm)) != 1) {
        if (code == gs_error_typecheck)
            pxm = 1;
        else if (code < 0)
            ecode = code;
        else
            pxm = !!pxm;
    }

    if (pcdev->bpc != bpc ||
        pcdev->num_components != num_comps) {

        gs_closedevice(pdev);

        pcdev->bpc = bpc;
        pcdev->num_components = num_comps;

        switch (num_comps * bpc) {
            case 1*1: case 1*2: case 1*4: case 1*8:
            case 4*4: case 4*8:
                pcdev->color_info.depth = bpc * num_comps;
                break;
            case 3*1:
                pcdev->color_info.depth = 4;
                break;
            case 3*2:
                pcdev->color_info.depth = 8;
                break;
            case 3*4:
                pcdev->color_info.depth = 16;
                break;
        }
        pcdev->color_info.max_gray = pcdev->color_info.max_color =
            (pcdev->color_info.dither_grays = pcdev->color_info.dither_colors = 1<<bpc) - 1;

        set_dev_proc(pcdev, map_cmyk_color, NULL);
        set_dev_proc(pcdev, map_rgb_color, NULL);
        set_dev_proc(pcdev, map_color_rgb, NULL);
        set_dev_proc(pcdev, encode_color,
                     num_comps == 1 ? chameleon_mono_encode_color :
                     num_comps == 4 ? chameleon_cmyk_encode_color :
                                      chameleon_rgb_encode_color);
        set_dev_proc(pcdev, decode_color,
                     num_comps == 1 ? chameleon_mono_decode_color :
                     num_comps == 4 ? chameleon_cmyk_decode_color :
                                      chameleon_rgb_decode_color);

        /* Reset the separable and linear shift, masks, bits. */
        set_linear_color_bits_mask_shift(pdev);
        pdev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
    }

    ecode = gdev_prn_put_params(pdev, plist);
    if (ecode < 0)
        return ecode;

    pcdev->dst_bpc = dst_bpc;
    pcdev->dst_num_components = dst_num_comps;
    pcdev->output_as_pxm = pxm;

    return 0;
}

static int
craprgbtocmyk(void  *arg,
              byte **dst,
              byte **src,
              int    w,
              int    h,
              int    raster)
{
    byte *d = *dst;
    byte *s = *src;

    while (w--) {
        int c = 255-*s++;
        int m = 255-*s++;
        int y = 255-*s++;
        int k = c;
        if (k > m)
            k = m;
        if (k > y)
            k = y;
        *d++ = c - k;
        *d++ = m - k;
        *d++ = y - k;
        *d++ = k;
    }

    return 0;
}

static int
header_4x1(FILE *file, gx_device_chameleon *pcdev)
{
    fprintf(file, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE CMYK\nENDHDR\n",
        pcdev->width, pcdev->height);
    return 0;
}

static int
write_4x1(const byte *data, int n, FILE *file)
{
    byte b[8];
    n -= 4;
    while (n > 0) {
        byte d = *data++;
        b[0] = (d & 128) ? 255 : 0;
        b[1] = (d &  64) ? 255 : 0;
        b[2] = (d &  32) ? 255 : 0;
        b[3] = (d &  16) ? 255 : 0;
        b[4] = (d &   8) ? 255 : 0;
        b[5] = (d &   4) ? 255 : 0;
        b[6] = (d &   2) ? 255 : 0;
        b[7] = (d &   1) ? 255 : 0;
        fwrite(b, 1, 8, file);
        n -= 8;
    }
    if (n == 0) {
        byte d = *data;
        b[0] = (d & 128) ? 255 : 0;
        b[1] = (d &  64) ? 255 : 0;
        b[2] = (d &  32) ? 255 : 0;
        b[3] = (d &  16) ? 255 : 0;
        fwrite(b, 1, 4, file);
    }
    return 0;
}

static int
header_3x8(FILE *file, gx_device_chameleon *pcdev)
{
    fprintf(file, "P6\n%d %d 255\n",
        pcdev->width, pcdev->height);
    return 0;
}

static int
do_fwrite(const byte *data, int n, FILE *file)
{
    return fwrite(data, 1, (n+7)>>3, file);
}

/* Send the page to the printer. */
static int
chameleon_print_page(gx_device_printer * pdev, FILE * prn_stream)
{				/* Just dump the bits on the file. */
    gx_device_chameleon *pcdev = (gx_device_chameleon *)pdev;
    /* If the file is 'nul', don't even do the writes. */
    int line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in;
    byte *data;
    int nul;
    int line_count = pdev->height;
    int i, code;
    gx_downscaler_t ds = { NULL };
    int depth = pdev->color_info.depth;
    int factor = pcdev->downscale.downscale_factor;
    int mfs = pcdev->downscale.min_feature_size;
    int pxm = pcdev->output_as_pxm;
    int dst_bpp;
    int (*write)(const byte *, int, FILE *) = do_fwrite;
    int (*header)(FILE *, gx_device_chameleon *) = NULL;
    int bitwidth;
    gx_downscale_cm_fn *col_convert = NULL;

    switch (pcdev->dst_num_components * pcdev->dst_bpc) {
        case 1*1: case 1*2: case 1*4: case 1*8:
        case 4*4: case 4*8: case 3*8:
            dst_bpp = pcdev->dst_num_components * pcdev->dst_bpc;
            break;
        case 3*1:
            dst_bpp = 4;
            break;
        case 3*2:
            dst_bpp = 8;
            break;
        case 3*4:
            dst_bpp = 12;
            break;
        default:
            return gs_error_rangecheck;
    }

    bitwidth = pdev->width * dst_bpp;
    line_size = (bitwidth+7)>>3;
    in = gs_alloc_bytes(pdev->memory, line_size, "chameleon_print_page(in)");
    if (in == 0)
        return_error(gs_error_VMerror);

    if (!strcmp(pdev->fname, "nul") || !strcmp(pdev->fname, "/dev/null"))
        write = NULL;
    else if (pxm) {
        switch (pcdev->dst_num_components) {
        case 4:
            if (pcdev->dst_bpc == 1)
                header = header_4x1, write = write_4x1;
            break;
        case 3:
            if (pcdev->dst_bpc == 8)
                header = header_3x8;
            break;
        }
    }

    if (pcdev->num_components == 3 && pcdev->bpc == 8 && pcdev->dst_num_components == 4)
        col_convert = craprgbtocmyk;

    code = gx_downscaler_init_trapped_cm_ets(&ds,
                                             (gx_device *)pdev,
                                             pcdev->bpc,
                                             pcdev->dst_bpc,
                                             pcdev->num_components,
                                             pcdev->downscale.downscale_factor,
                                             pcdev->downscale.min_feature_size,
                                             NULL, 0, /* Adjust width */
                                             0, 0, NULL, /* Trapping w/h/comp_order */
                                             col_convert, NULL, /* Color Management */
                                             pcdev->dst_num_components,
                                             0); /* Not ETS for now */
    if (code < 0)
        goto cleanup;

    if (header) {
        code = header(prn_stream, pcdev);
        if (code < 0)
            goto cleanup;
    }

    for (i = 0; i < line_count; i++) {
        code = gx_downscaler_getbits(&ds, in, i);
        if (code < 0)
            break;
        if (write != NULL)
            write(in, bitwidth, prn_stream);
    }
    gx_downscaler_fin(&ds);
cleanup:
    gs_free_object(pdev->memory, in, "chameleon_print_page(in)");
    return code;
}

static int
chameleon_put_image(gx_device *pdev, const byte **buffers, int num_chan, int xstart,
              int ystart, int width, int height, int row_stride,
              int alpha_plane_index, int tag_plane_index)
{
    gx_device_memory *pmemdev = (gx_device_memory *)pdev;
    byte *buffer_prn;
    int yend = ystart + height;
    int xend = xstart + width;
    int x, y, k;
    int src_position, des_position;

    if (alpha_plane_index != 0)
        return 0;                /* we don't want alpha, return 0 to ask for the    */
                                 /* pdf14 device to do the alpha composition        */
    if (num_chan != 3 || tag_plane_index <= 0 || pdev->color_info.depth != 24 || pdev->color_info.num_components != 3)
        return_error(gs_error_unknownerror);        /* can't handle these cases */
    /* Drill down to get the appropriate memory buffer pointer */
    buffer_prn = pmemdev->base;
    /* Now go ahead and fill */
    for ( y = ystart; y < yend; y++ ) {
        src_position = (y - ystart) * row_stride;
        des_position = y * pmemdev->raster + xstart * 4;
        for ( x = xstart; x < xend; x++ ) {
            /* Tag data first, then RGB */
            buffer_prn[des_position] =
                buffers[tag_plane_index][src_position];
                des_position += 1;
            for ( k = 0; k < 3; k++) {
                buffer_prn[des_position] =
                    buffers[k][src_position];
                    des_position += 1;
            }
            src_position += 1;
        }
    }
    return height;        /* we used all of the data */
}
