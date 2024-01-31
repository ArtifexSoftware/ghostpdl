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


#include "gdevprn.h"
#include "gdevmem.h"
#include "gdevpccm.h"
#include "gscdefs.h"
#include "gxgetbit.h"
#include "tessocr.h"
#include "gxdownscale.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_initialize_device(ocr_initialize_device);
static dev_proc_print_page(ocr_print_page);
static dev_proc_print_page(hocr_print_page);
static dev_proc_get_params(ocr_get_params);
static dev_proc_put_params(ocr_put_params);
static dev_proc_open_device(ocr_open);
static dev_proc_close_device(ocr_close);
static dev_proc_close_device(hocr_close);

typedef struct gx_device_ocr_s gx_device_ocr;
struct gx_device_ocr_s {
    gx_device_common;
    gx_prn_device_common;
    gx_downscaler_params downscale;
    char language[1024];
    int engine;
    int page_count;
    void *api;
};

/* 8-bit gray bitmap -> UTF8 OCRd text */
static void
ocr_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);

    set_dev_proc(dev, initialize_device, ocr_initialize_device);
    set_dev_proc(dev, open_device, ocr_open);
    set_dev_proc(dev, close_device, ocr_close);
    set_dev_proc(dev, get_params, ocr_get_params);
    set_dev_proc(dev, put_params, ocr_put_params);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_ocr gs_ocr_device =
{
    prn_device_body(gx_device_ocr, ocr_initialize_device_procs, "ocr",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0,	/* margins */
                    1, 8, 255, 0, 256, 0, ocr_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* 8-bit gray bitmap -> HTML OCRd text */
static void
hocr_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray_bg(dev);

    set_dev_proc(dev, initialize_device, ocr_initialize_device);
    set_dev_proc(dev, open_device, ocr_open);
    set_dev_proc(dev, close_device, hocr_close);
    set_dev_proc(dev, get_params, ocr_get_params);
    set_dev_proc(dev, put_params, ocr_put_params);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_ocr gs_hocr_device =
{
    prn_device_body(gx_device_ocr, hocr_initialize_device_procs, "hocr",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    X_DPI, Y_DPI,
                    0, 0, 0, 0,	/* margins */
                    1, 8, 255, 0, 256, 0, hocr_print_page),
    GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* ------ Private definitions ------ */

#define HOCR_HEADER "<html>\n <body>\n"
#define HOCR_TRAILER " </body>\n</html>\n"

static int
ocr_initialize_device(gx_device *dev)
{
    gx_device_ocr *odev = (gx_device_ocr *)dev;
    const char *default_ocr_lang = "eng";

    odev->language[0] = '\0';
    strcpy(odev->language, default_ocr_lang);
    return 0;
}

static int
ocr_open(gx_device *pdev)
{
    gx_device_ocr *dev = (gx_device_ocr *)pdev;
    int code;

    dev->page_count = 0;

    code = ocr_init_api(dev->memory->non_gc_memory,
                        dev->language, dev->engine, &dev->api);
    if (code < 0)
        return code;

    return gdev_prn_open(pdev);
}

static int
ocr_close(gx_device *pdev)
{
    gx_device_ocr *dev = (gx_device_ocr *)pdev;

    ocr_fin_api(dev->memory->non_gc_memory, dev->api);

    return gdev_prn_close(pdev);
}

static int
hocr_close(gx_device *pdev)
{
    gx_device_ocr *dev = (gx_device_ocr *)pdev;

    if (dev->page_count > 0 && dev->file != NULL) {
       gp_fwrite(HOCR_TRAILER, 1, sizeof(HOCR_TRAILER)-1, dev->file);
    }

    return ocr_close(pdev);
}

static int
ocr_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_ocr *pdev = (gx_device_ocr *)dev;
    int code, ecode = 0;
    gs_param_string langstr;

    if (pdev->language[0]) {
        langstr.data = (const byte *)pdev->language;
        langstr.size = strlen(pdev->language);
        langstr.persistent = false;
    } else {
        langstr.data = (const byte *)"eng";
        langstr.size = 3;
        langstr.persistent = false;
    }
    if ((code = param_write_string(plist, "OCRLanguage", &langstr)) < 0)
        ecode = code;

    if ((code = param_write_int(plist, "OCREngine", &pdev->engine)) < 0)
        ecode = code;

    if ((code = gx_downscaler_write_params(plist, &pdev->downscale,
                                           GX_DOWNSCALER_PARAMS_MFS)) < 0)
        ecode = code;

    code = gdev_prn_get_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
ocr_put_params(gx_device *dev, gs_param_list *plist)
{
    gx_device_ocr *pdev = (gx_device_ocr *)dev;
    int code, ecode = 0;
    gs_param_string langstr;
    const char *param_name;
    size_t len;
    int engine;

    switch (code = param_read_string(plist, (param_name = "OCRLanguage"), &langstr)) {
        case 0:
                if (pdev->memory->gs_lib_ctx->core->path_control_active
                && (strlen(pdev->language) != langstr.size || memcmp(pdev->language, langstr.data, langstr.size) != 0)) {
                return_error(gs_error_invalidaccess);
            }
            else {
                len = langstr.size;
                if (len >= sizeof(pdev->language))
                    len = sizeof(pdev->language)-1;
                memcpy(pdev->language, langstr.data, len);
                pdev->language[len] = 0;
            }
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }

    switch (code = param_read_int(plist, (param_name = "OCREngine"), &engine)) {
        case 0:
            pdev->engine = engine;
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }

    code = gx_downscaler_read_params(plist, &pdev->downscale,
                                     GX_DOWNSCALER_PARAMS_MFS);
    if (code < 0)
    {
        ecode = code;
        param_signal_error(plist, param_name, ecode);
    }

    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

/* OCR a page and write it out. */
static int
do_ocr_print_page(gx_device_ocr * pdev, gp_file * file, int hocr)
{
    int row;
    byte *data = NULL;
    char *out;
    int factor = pdev->downscale.downscale_factor;
    int height = gx_downscaler_scale(pdev->height, factor);
    int width = gx_downscaler_scale(pdev->width, factor);
    int raster = bitmap_raster(width*8);
    gx_downscaler_t ds;
    int code;

    code = gx_downscaler_init(&ds,
                              (gx_device *)pdev,
                              8, /* src_bpc */
                              8, /* dst_bpc */
                              1, /* num_comps */
                              &pdev->downscale,
                              NULL, 0); /* adjust_width_proc, width */
    if (code < 0)
        return code;

    data = gs_alloc_bytes(pdev->memory,
                          raster * height,
                          "do_ocr_print_page");
    if (data == NULL) {
        gx_downscaler_fin(&ds);
        code = gs_error_VMerror;
        goto done;
    }

    for (row = 0; row < height && code >= 0; row++) {
        code = gx_downscaler_getbits(&ds, data + row * raster, row);
    }
    gx_downscaler_fin(&ds);
    if (code < 0)
        goto done;

    if (hocr)
        code = ocr_image_to_hocr(pdev->api,
                                 width, height,
                                 8, raster,
                                 (int)pdev->HWResolution[0],
                                 (int)pdev->HWResolution[1],
                                 data, 0, pdev->page_count,
                                 &out);
    else
        code = ocr_image_to_utf8(pdev->api,
                                 width, height,
                                 8, raster,
                                 (int)pdev->HWResolution[0],
                                 (int)pdev->HWResolution[1],
                                 data, 0, &out);
    if (code < 0)
        goto done;
    if (out)
    {
        if (hocr && pdev->page_count == 0) {
            gp_fwrite(HOCR_HEADER, 1, sizeof(HOCR_HEADER)-1, file);
        }
        gp_fwrite(out, 1, strlen(out), file);
        gs_free_object(pdev->memory->non_gc_memory,
                       out, "ocr_image_to_utf8");
    }

  done:
    if (data)
        gs_free_object(pdev->memory, data, "do_ocr_print_page");
    pdev->page_count++;

    return code;
}

static int
ocr_print_page(gx_device_printer * pdev, gp_file * file)
{
    return do_ocr_print_page((gx_device_ocr *)pdev, file, 0);
}

static int
hocr_print_page(gx_device_printer * pdev, gp_file * file)
{
    return do_ocr_print_page((gx_device_ocr *)pdev, file, 1);
}
