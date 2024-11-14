/* Copyright (C) 2024 Artifex Software, Inc.
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


/* Parallel processing to PPM Format. */

#include "gdevprn.h"
#include "gdevmem.h"
#include "gscdefs.h"
#include "gxgetbit.h"
#include "gxdownscale.h"
#include "gxdevsop.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_print_page(pppm_print_page);

typedef struct gx_device_pppm_s gx_device_pppm;
struct gx_device_pppm_s {
    gx_device_common;
    gx_prn_device_common;
    gx_downscaler_params downscale;
    gs_offset_t header_len;
    gx_monitor_t *monitor;
};

static int
pppm_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_pppm *pdev = (gx_device_pppm *)dev;
    gs_param_list * plist = (gs_param_list *)list;

    if (strcmp(Param, "DownScaleFactor") == 0) {
        return param_write_int(plist, "DownScaleFactor", &pdev->downscale.downscale_factor);
    }
    return gdev_prn_get_param(dev, Param, list);
}

static int
pppm_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_pppm *pdev = (gx_device_pppm *)dev;
    int code, ecode;

    ecode = 0;
    if ((code = gx_downscaler_write_params(plist, &pdev->downscale, 0)) < 0)
        ecode = code;

    code = gdev_prn_get_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
pppm_put_params(gx_device *dev, gs_param_list *plist)
{
    gx_device_pppm *pdev = (gx_device_pppm *)dev;
    int code, ecode;

    ecode = gx_downscaler_read_params(plist, &pdev->downscale, 0);

    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
pppm_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    gx_device_pppm *fdev = (gx_device_pppm *)pdev;

    if (dev_spec_op == gxdso_adjust_bandheight)
        return gx_downscaler_adjust_bandheight(fdev->downscale.downscale_factor, size);

    if (dev_spec_op == gxdso_get_dev_param) {
        int code;
        dev_param_req_t *request = (dev_param_req_t *)data;
        code = pppm_get_param(pdev, request->Param, request->list);
        if (code != gs_error_undefined)
            return code;
    }

    return gdev_prn_dev_spec_op(pdev, dev_spec_op, data, size);
}

/* 24-bit color. */

/* Since the print_page doesn't alter the device, this device can print in the background */
static void
pppm_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb_bg(dev);

    set_dev_proc(dev, get_params, pppm_get_params);
    set_dev_proc(dev, put_params, pppm_put_params);
    set_dev_proc(dev, dev_spec_op, pppm_dev_spec_op);
}

const gx_device_pppm gs_pppm_device =
{prn_device_body(gx_device_pppm, pppm_initialize_device_procs, "pppm",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 24, 255, 255, 256, 256, pppm_print_page),
                 GX_DOWNSCALER_PARAMS_DEFAULTS
};

/* ------ Private definitions ------ */

static int pppm_process_and_output(void *arg, gx_device *dev, gx_device *bdev, const gs_int_rect *rect, void *buffer_)
{
    gp_file *file = (gp_file *)arg;
    int code;
    gx_device_pppm *fdev = (gx_device_pppm *)dev;
    gs_get_bits_params_t params;
    int w = rect->q.x - rect->p.x;
    int raster = bitmap_raster(bdev->width * 3 * 8);
    int h = rect->q.y - rect->p.y;
    int y;
    unsigned char *p;
    gs_int_rect my_rect;
    gs_offset_t offset = fdev->header_len;
    gx_device_clist_reader *clrdev = (gx_device_clist_reader *)dev;
    gx_device_clist_reader *orig_dev = NULL;
    gx_device_pppm *orig_fdev = NULL;

    if (h <= 0 || w <= 0)
        return 0;

    if (offset == 0) {
        /* We are running in a copied device. Must be a clist worker. */
        orig_dev = clrdev->orig_clist_device;
        orig_fdev = (gx_device_pppm *)orig_dev;
        offset = orig_fdev->header_len;
    }
    offset += ((gs_offset_t)w) * 3 * rect->p.y;

    /* Process the data for a single band. */
    params.options = GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_CHUNKY | GB_RETURN_POINTER | GB_ALIGN_ANY | GB_OFFSET_0 | GB_RASTER_ANY;
    my_rect.p.x = 0;
    my_rect.p.y = 0;
    my_rect.q.x = w;
    my_rect.q.y = h;
    code = dev_proc(bdev, get_bits_rectangle)(bdev, &my_rect, &params);
    if (code < 0)
        return code;

    p = params.data[0];
    for (y = h; y > 0; y--)
    {
        gp_fpwrite(p, w*3, offset, file);
        offset += w*3;
        p += raster;
    }

    /* If we are running in clist multi-threaded mode, we need to nominate the next band that we should work with. */
    if (orig_dev) {
        code = gx_monitor_enter(orig_fdev->monitor);
        if (code < 0)
            return code;

        clrdev->next_band = orig_dev->next_band;
        orig_dev->next_band += orig_dev->thread_lookahead_direction;
        code = gx_monitor_leave(orig_fdev->monitor);
        if (code < 0)
            return code;
    }

    return code;
}

/* Write out a page in PNG format. */
static int
pppm_print_page(gx_device_printer *pdev, gp_file *file)
{
    gx_device_pppm *fdev = (gx_device_pppm *)pdev;
    static const unsigned char ppmsig[] = { 'P', '6', '\n' };
    char text[32];
    gx_process_page_options_t process = { 0 };
    int code;

    if (!gp_can_share_fdesc()) {
        emprintf(pdev->memory, "The pppm device relies on being able to call gp_fpwrite, and this is not supported on this platform.\n\n");
        return_error(gs_error_ioerror);
    }

    gp_fwrite(ppmsig, 1, sizeof(ppmsig), file); /* Signature */
    snprintf(text, sizeof(text), "%d\n", gx_downscaler_scale_rounded(pdev->width, fdev->downscale.downscale_factor));
    gp_fwrite(text, 1, strlen(text), file);
    snprintf(text, sizeof(text), "%d\n", gx_downscaler_scale_rounded(pdev->height, fdev->downscale.downscale_factor));
    gp_fwrite(text, 1, strlen(text), file);
    gp_fwrite((unsigned char *)"255\n", 1, 4, file);
    fdev->header_len = gp_ftell(file);
    gp_fflush(file);

    process.init_buffer_fn = NULL;
    process.free_buffer_fn = NULL;
    process.process_fn = pppm_process_and_output;
    process.output_fn = NULL;
    process.arg = file;

    fdev->monitor = gx_monitor_alloc(fdev->memory);
    if (fdev->monitor == NULL)
        return_error(gs_error_VMerror);

    code = gx_downscaler_process_page((gx_device *)fdev, &process, fdev->downscale.downscale_factor);
    gx_monitor_free(fdev->monitor);

    return code;
}
