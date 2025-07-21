/* Copyright (C) 2001-2025 Artifex Software, Inc.
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

/* JPEG output driver */
#include "stdio_.h"		/* for jpeglib.h */
#include "jpeglib_.h"
#include "gdevprn.h"
#include "stream.h"
#include "strimpl.h"
#include "sdct.h"
#include "sjpeg.h"
#include "gxdownscale.h"

/* Structure for the JPEG-writing device. */
typedef struct gx_device_jpeg_s {
    gx_device_common;
    gx_prn_device_common;
    /* Additional parameters */
    int JPEGQ;			/* quality on IJG scale */
    float QFactor;		/* quality per DCTEncode conventions */
    /* JPEGQ overrides QFactor if both are specified. */
    bool EmbedProfile;

    /** 1.0 default 2.0 is twice as big
     */
    gs_point ViewScale;

    /** translation needs to have scalefactor multiplied in.
     */
    gs_point ViewTrans;

    gx_downscaler_params downscale;
} gx_device_jpeg;

/* The device descriptor */
static dev_proc_get_params(jpeg_get_params);
static dev_proc_get_initial_matrix(jpeg_get_initial_matrix);
static dev_proc_put_params(jpeg_put_params);
static dev_proc_print_page(jpeg_print_page);
static dev_proc_map_color_rgb(jpegcmyk_map_color_rgb);
static dev_proc_map_cmyk_color(jpegcmyk_map_cmyk_color);
static dev_proc_decode_color(jpegcmyk_decode_color);

/* ------ The device descriptors ------ */

/* Default X and Y resolution. */
#ifndef X_DPI
#  define X_DPI 72
#endif
#ifndef Y_DPI
#  define Y_DPI 72
#endif

/* 24-bit color */

static void
jpeg_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb_bg(dev);

    set_dev_proc(dev, get_initial_matrix, jpeg_get_initial_matrix);
    set_dev_proc(dev, get_params, jpeg_get_params);
    set_dev_proc(dev, put_params, jpeg_put_params);
}

const gx_device_jpeg gs_jpeg_device =
{prn_device_std_body(gx_device_jpeg, jpeg_initialize_device_procs, "jpeg",
                     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                     X_DPI, Y_DPI, 0, 0, 0, 0, 24, jpeg_print_page),
 0,				/* JPEGQ: 0 indicates not specified */
 0.0,				/* QFactor: 0 indicates not specified */
 1,                 /* Embed an ICC profile in the output */
 { 1.0, 1.0 },                  /* ViewScale 1 to 1 */
 { 0.0, 0.0 },                  /* translation 0 */
 GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 8-bit gray */

static void
jpeggray_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);

    set_dev_proc(dev, get_initial_matrix, jpeg_get_initial_matrix);
    set_dev_proc(dev, get_params, jpeg_get_params);
    set_dev_proc(dev, put_params, jpeg_put_params);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_jpeg gs_jpeggray_device =
{prn_device_body(gx_device_jpeg, jpeggray_initialize_device_procs, "jpeggray",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI, 0, 0, 0, 0,
                 1, 8, 255, 0, 256, 0,
                 jpeg_print_page),
 0,				/* JPEGQ: 0 indicates not specified */
 0.0,				/* QFactor: 0 indicates not specified */
 1,                 /* Embed an ICC profile in the output */
 { 1.0, 1.0 },                  /* ViewScale 1 to 1 */
 { 0.0, 0.0 },                   /* translation 0 */
 GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 32-bit CMYK */

static void
jpegcmyk_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_bg(dev);

    set_dev_proc(dev, get_initial_matrix, jpeg_get_initial_matrix);
    set_dev_proc(dev, map_color_rgb, jpegcmyk_map_color_rgb);
    set_dev_proc(dev, get_params, jpeg_get_params);
    set_dev_proc(dev, put_params, jpeg_put_params);
    set_dev_proc(dev, map_cmyk_color, jpegcmyk_map_cmyk_color);

    set_dev_proc(dev, encode_color, jpegcmyk_map_cmyk_color);
    set_dev_proc(dev, decode_color, jpegcmyk_decode_color);
}

const gx_device_jpeg gs_jpegcmyk_device =
{prn_device_std_body(gx_device_jpeg, jpegcmyk_initialize_device_procs, "jpegcmyk",
                     DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                     X_DPI, Y_DPI, 0, 0, 0, 0, 32, jpeg_print_page),
 0,				/* JPEGQ: 0 indicates not specified */
 0.0,				/* QFactor: 0 indicates not specified */
 1,                 /* Embed an ICC profile in the output */
 { 1.0, 1.0 },                  /* ViewScale 1 to 1 */
 { 0.0, 0.0 },                   /* translation 0 */
 GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* Apparently Adobe Photoshop and some other applications that	*/
/* accept JPEG CMYK images expect color values to be inverted.	*/
static int
jpegcmyk_map_color_rgb(gx_device * dev, gx_color_index color,
                        gx_color_value prgb[3])
{
    int
        not_k = color & 0xff,
        r = not_k - ~(color >> 24),
        g = not_k - ~((color >> 16) & 0xff),
        b = not_k - ~((color >> 8) & 0xff);

    prgb[0] = (r < 0 ? 0 : gx_color_value_from_byte(r));
    prgb[1] = (g < 0 ? 0 : gx_color_value_from_byte(g));
    prgb[2] = (b < 0 ? 0 : gx_color_value_from_byte(b));
    return 0;
}

static gx_color_index
jpegcmyk_map_cmyk_color(gx_device * dev, const gx_color_value cv[])
{
    gx_color_index color = ~(
        gx_color_value_to_byte(cv[3]) +
        ((uint)gx_color_value_to_byte(cv[2]) << 8) +
        ((uint)gx_color_value_to_byte(cv[1]) << 16) +
        ((uint)gx_color_value_to_byte(cv[0]) << 24));

    return (color == gx_no_color_index ? color ^ 1 : color);
}

static int
jpegcmyk_decode_color(gx_device * dev, gx_color_index color,
                      gx_color_value cv[])
{
    color = ~color;
    cv[0] = gx_color_value_from_byte(0xff & (color>>24));
    cv[1] = gx_color_value_from_byte(0xff & (color>>16));
    cv[2] = gx_color_value_from_byte(0xff & (color>>8));
    cv[3] = gx_color_value_from_byte(0xff & color);

    return 0;
}

/* Get parameters. */
static int
jpeg_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_jpeg *jdev = (gx_device_jpeg *) dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode;
    float float2double;
    if (code < 0)
        return code;

    ecode = 0;
    if ((code = gx_downscaler_write_params(plist, &jdev->downscale, 0)) < 0)
        ecode = code;
    if ((ecode = param_write_int(plist, "JPEGQ", &jdev->JPEGQ)) < 0)
        code = ecode;
    if ((ecode = param_write_bool(plist, "EmbedProfile", &jdev->EmbedProfile)) < 0)
        code = ecode;
    if ((ecode = param_write_float(plist, "QFactor", &jdev->QFactor)) < 0)
        code = ecode;
    float2double = jdev->ViewScale.x;
    if ((ecode = param_write_float(plist, "ViewScaleX", &float2double)) < 0)
        code = ecode;
    float2double = jdev->ViewScale.y;
    if ((ecode = param_write_float(plist, "ViewScaleY", &float2double)) < 0)
        code = ecode;
    float2double = jdev->ViewTrans.x;
    if ((ecode = param_write_float(plist, "ViewTransX", &float2double)) < 0)
        code = ecode;
    float2double = jdev->ViewTrans.y;
    if ((ecode = param_write_float(plist, "ViewTransY", &float2double)) < 0)
        code = ecode;

    return code;
}

/* Put parameters. */
static int
jpeg_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_jpeg *jdev = (gx_device_jpeg *) dev;
    int ecode = 0;
    int code;
    gs_param_name param_name;
    int jq = jdev->JPEGQ;
    float qf = jdev->QFactor;
    float fparam;
    bool embed_profile = jdev->EmbedProfile, bparam;

    ecode = gx_downscaler_read_params(plist, &jdev->downscale, 0);

    switch (code = param_read_int(plist, (param_name = "JPEGQ"), &jq)) {
        case 0:
            if (jq < 0 || jq > 100)
                ecode = gs_error_limitcheck;
            else
                break;
            goto jqe;
        default:
            ecode = code;
          jqe:param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }

    switch (code = param_read_float(plist, (param_name = "QFactor"), &qf)) {
        case 0:
            if (qf < 0.0 || qf > 1.0e6)
                ecode = gs_error_limitcheck;
            else
                break;
            goto qfe;
        default:
            ecode = code;
          qfe:param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }

    code = param_read_float(plist, (param_name = "ViewScaleX"), &fparam);
    if ( code == 0 ) {
        if (fparam < 1.0)
            param_signal_error(plist, param_name, gs_error_limitcheck);
        else
            jdev->ViewScale.x = fparam;
    }
    else if ( code < 1 ) {
        ecode = code;
        param_signal_error(plist, param_name, code);
    }

    code = param_read_float(plist, (param_name = "ViewScaleY"), &fparam);
    if ( code == 0 ) {
        if (fparam < 1.0)
            param_signal_error(plist, param_name, gs_error_limitcheck);
        else
            jdev->ViewScale.y = fparam;
    }
    else if ( code < 1 ) {
        ecode = code;
        param_signal_error(plist, param_name, code);
    }

    /* pixels in desired dpi, auto negative ( moves up and left ) */
    code = param_read_float(plist, (param_name = "ViewTransX"), &fparam);
    if ( code == 0 ) {
        jdev->ViewTrans.x = fparam;
    }
    else if ( code < 1 ) {
        ecode = code;
        param_signal_error(plist, param_name, code);
    }

    code = param_read_float(plist, (param_name = "ViewTransY"), &fparam);
    if ( code == 0 ) {
        jdev->ViewTrans.y = fparam;
    }
    else if ( code < 1 ) {
        ecode = code;
        param_signal_error(plist, param_name, code);
    }

    code = param_read_bool(plist, (param_name = "EmbedProfile"), &bparam);
    if ( code == 0 ) {
        jdev->EmbedProfile = bparam;
    }
    else if ( code < 1 ) {
        ecode = code;
        param_signal_error(plist, param_name, code);
    }

    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        return code;

    if (ecode < 0)
        return ecode;

    jdev->JPEGQ = jq;
    jdev->QFactor = qf;
    return 0;
}

/******************************************************************
 This device supports translation and scaling.

0123456

0PPPPPPP  0 is origin
PPPPPPPP  1 is x1,y1 (2,2)
PP1vvvPP  2 is x2,y2 (6,6)
PPvvvvPP  v is viewport, P is original page
PPvvvvPP
PPPPPP2P
PPPPPPPP

Given a view port in pixels starting at x1,y1
where x1 < width, y1 < height in pixels

ViewScaleX = desired Resolution / HWResolution ; 1.0 default
ViewScaleY = desired Resolution / HWResolution

HWResolutionX = desired dpi at 1:1 scaling     ; 72dpi default
HWResolutionY = desired dpi at 1:1 scaling

ViewTransX = x1 * ViewScaleX                   ; 0.0 default
ViewTransY = y1 * ViewScaleY

if initial matrix multiplies ViewScaleX in then translation is limited to
multiples of the HWResolution.

***************************************************************************/

static void
jpeg_get_initial_matrix(gx_device *dev, gs_matrix *pmat)
{
    gx_device_jpeg *pdev = (gx_device_jpeg *)dev;
    double fs_res = (dev->HWResolution[0] / 72.0) * pdev->ViewScale.x;
    double ss_res = (dev->HWResolution[1] / 72.0) * pdev->ViewScale.y;

    /* NB this device has no paper margins */

    switch(pdev->LeadingEdge & LEADINGEDGE_MASK) {
    case 1:
        pmat->xx = 0;
        pmat->xy = -ss_res;
        pmat->yx = -fs_res;
        pmat->yy = 0;
        pmat->tx = (pdev->width * pdev->ViewScale.x) - pdev->ViewTrans.x;
        pmat->ty = (pdev->height * pdev->ViewScale.y) - pdev->ViewTrans.y;
        break;
    case 2:
        pmat->xx = -fs_res;
        pmat->xy = 0;
        pmat->yx = 0;
        pmat->yy = ss_res;
        pmat->tx = (pdev->width * pdev->ViewScale.x) - pdev->ViewTrans.x;
        pmat->ty = -pdev->ViewTrans.x;
        break;
    case 3:
        pmat->xx = 0;
        pmat->xy = ss_res;
        pmat->yx = fs_res;
        pmat->yy = 0;
        pmat->tx = -pdev->ViewTrans.x;
        pmat->ty = -pdev->ViewTrans.y;
        break;
    default:
    case 0:
        pmat->xx = fs_res;
        pmat->xy = 0;
        pmat->yx = 0;
        pmat->yy = -ss_res;
        pmat->tx = -pdev->ViewTrans.x;
        pmat->ty = (pdev->height * pdev->ViewScale.y) - pdev->ViewTrans.y;
        break;
    }

}

/* Send the page to the file. */
static int
jpeg_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    gx_device_jpeg *jdev = (gx_device_jpeg *) pdev;
    gs_memory_t *mem = pdev->memory;
    size_t line_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    byte *in = gs_alloc_bytes(mem, line_size, "jpeg_print_page(in)");
    jpeg_compress_data *jcdp = gs_alloc_struct_immovable(mem, jpeg_compress_data,
      &st_jpeg_compress_data, "jpeg_print_page(jpeg_compress_data)");
    byte *fbuf = 0;
    uint fbuf_size;
    byte *jbuf = 0;
    uint jbuf_size;
    int lnum;
    int code;
    stream_DCT_state state;
    stream fstrm, jstrm;
    gx_downscaler_t ds;

    if (jcdp == 0 || in == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto fail;
    }
    code = gx_downscaler_init(&ds, (gx_device *)jdev, 8, 8,
                              jdev->color_info.depth/8,
                              &jdev->downscale, NULL, 0);
    if (code < 0) {
        gs_free_object(mem, jcdp, "jpeg_print_page(jpeg_compress_data)");
        jcdp = NULL;
        goto fail;
    }

    /* Create the DCT encoder state. */
    jcdp->templat = s_DCTE_template;
    s_init_state((stream_state *)&state, &jcdp->templat, 0);
    if (state.templat->set_defaults) {
        state.memory = mem;
        (*state.templat->set_defaults) ((stream_state *) & state);
        state.memory = NULL;
    }
    state.QFactor = 1.0;	/* disable quality adjustment in zfdcte.c */
    state.ColorTransform = 1;	/* default for RGB */
    /* We insert no markers, allowing the IJG library to emit */
    /* the format it thinks best. */
    state.NoMarker = true;	/* do not insert our own Adobe marker */
    state.Markers.data = 0;
    state.Markers.size = 0;
    state.data.compress = jcdp;
    /* Add in ICC profile */
    state.icc_profile = NULL; /* In case it is not set here */
    if (pdev->icc_struct != NULL &&
        pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE] != NULL) {
        cmm_profile_t *icc_profile = pdev->icc_struct->device_profile[GS_DEFAULT_DEVICE_PROFILE];
        if (icc_profile->num_comps == pdev->color_info.num_components &&
            !(pdev->icc_struct->usefastcolor) && jdev->EmbedProfile) {
            state.icc_profile = icc_profile;
        }
    }
    /* We need state.memory for gs_jpeg_create_compress().... */
    jcdp->memory = state.jpeg_memory = state.memory = mem;
    if ((code = gs_jpeg_create_compress(&state)) < 0)
    {
        gx_downscaler_fin(&ds);
        goto fail;
    }
    /* ....but we need it to be NULL so we don't try to free
     * the stack based state...
     */
    state.memory = NULL;
    jcdp->cinfo.image_width = gx_downscaler_scale(pdev->width, jdev->downscale.downscale_factor);
    jcdp->cinfo.image_height = gx_downscaler_scale(pdev->height, jdev->downscale.downscale_factor);
    switch (pdev->color_info.depth) {
        case 32:
            jcdp->cinfo.input_components = 4;
            jcdp->cinfo.in_color_space = JCS_CMYK;
            break;
        case 24:
            jcdp->cinfo.input_components = 3;
            jcdp->cinfo.in_color_space = JCS_RGB;
            break;
        case 8:
            jcdp->cinfo.input_components = 1;
            jcdp->cinfo.in_color_space = JCS_GRAYSCALE;
            break;
    }
    /* Set compression parameters. */
    if ((code = gs_jpeg_set_defaults(&state)) < 0)
        goto done;
    if (jdev->JPEGQ > 0) {
        code = gs_jpeg_set_quality(&state, jdev->JPEGQ, TRUE);
        if (code < 0)
            goto done;
    } else if (jdev->QFactor > 0.0) {
        code = gs_jpeg_set_linear_quality(&state,
                                          (int)(min(jdev->QFactor, 100.0)
                                                * 100.0 + 0.5),
                                          TRUE);
        if (code < 0)
            goto done;
    }
    jcdp->cinfo.restart_interval = 0;
    jcdp->cinfo.density_unit = 1;	/* dots/inch (no #define or enum) */
    jcdp->cinfo.X_density = (UINT16)pdev->HWResolution[0];
    jcdp->cinfo.Y_density = (UINT16)pdev->HWResolution[1];
    /* Create the filter. */
    /* Make sure we get at least a full scan line of input. */
    state.scan_line_size = jcdp->cinfo.input_components *
        jcdp->cinfo.image_width;
    jcdp->templat.min_in_size =
        max(s_DCTE_template.min_in_size, state.scan_line_size);
    /* Make sure we can write the user markers in a single go. */
    jcdp->templat.min_out_size =
        max(s_DCTE_template.min_out_size, state.Markers.size);

    /* Set up the streams. */
    fbuf_size = max(512 /* arbitrary */ , jcdp->templat.min_out_size);
    jbuf_size = jcdp->templat.min_in_size;
    if ((fbuf = gs_alloc_bytes(mem, fbuf_size, "jpeg_print_page(fbuf)")) == 0 ||
        (jbuf = gs_alloc_bytes(mem, jbuf_size, "jpeg_print_page(jbuf)")) == 0
        ) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    s_init(&fstrm, mem);
    swrite_file(&fstrm, prn_stream, fbuf, fbuf_size);
    s_init(&jstrm, mem);
    s_std_init(&jstrm, jbuf, jbuf_size, &s_filter_write_procs,
               s_mode_write);
    jstrm.state = (stream_state *) & state;
    jstrm.procs.process = state.templat->process;
    jstrm.strm = &fstrm;
    if (state.templat->init)
        (*state.templat->init) (jstrm.state);

    /* Copy the data to the output. */
    for (lnum = 0; lnum < jcdp->cinfo.image_height; ++lnum) {
        uint ignore_used;

        if (jstrm.end_status) {
            code = gs_note_error(gs_error_ioerror);
            goto done;
        }
        gx_downscaler_getbits(&ds, in, lnum);
        sputs(&jstrm, in, state.scan_line_size, &ignore_used);
    }

    /* Wrap up. */
    sclose(&jstrm);
    sflush(&fstrm);
  done:
    gs_free_object(mem, jbuf, "jpeg_print_page(jbuf)");
    gs_free_object(mem, fbuf, "jpeg_print_page(fbuf)");
    if (jcdp) {
        gs_jpeg_destroy(&state);
        gs_free_object(mem, jcdp, "jpeg_print_page(jpeg_compress_data)");
    }
    gx_downscaler_fin(&ds);
    gs_free_object(mem, in, "jpeg_print_page(in)");
    return code;
  fail:
    if (jcdp)
        gs_free_object(mem, jcdp, "jpeg_print_page(jpeg_compress_data)");
    gs_free_object(mem, in, "jpeg_print_page(in)");
    return code;
}
