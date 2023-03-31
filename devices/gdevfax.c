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

/* Fax devices */
#include "gdevprn.h"
#include "strimpl.h"
#include "scfx.h"
#include "gdevfax.h"
#include "minftrsz.h"

/* The device descriptors */
static dev_proc_print_page(faxg3_print_page);
static dev_proc_print_page(faxg32d_print_page);
static dev_proc_print_page(faxg4_print_page);

/* Define procedures that adjust the paper size. */
/* Since the print_page doesn't alter the device, this device can print in the background */
static void
fax_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_mono_bg(dev);

    set_dev_proc(dev, open_device, gdev_prn_open);
    set_dev_proc(dev, get_params, gdev_fax_get_params);
    set_dev_proc(dev, put_params, gdev_fax_put_params);
}

#define FAX_DEVICE(dname, print_page)\
{\
    FAX_DEVICE_BODY(gx_device_fax, fax_initialize_device_procs, dname, print_page)\
}

const gx_device_fax gs_faxg3_device =
    FAX_DEVICE("faxg3", faxg3_print_page);

const gx_device_fax gs_faxg32d_device =
    FAX_DEVICE("faxg32d", faxg32d_print_page);

const gx_device_fax gs_faxg4_device =
    FAX_DEVICE("faxg4", faxg4_print_page);

/* Open the device. */
/* This is no longer needed: we retain it for client backward compatibility. */
int
gdev_fax_open(gx_device * dev)
{
    return gdev_prn_open(dev);
}

/* Get/put the fax parameters: AdjustWidth and MinFeatureSize */
int
gdev_fax_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_fax *const fdev = (gx_device_fax *)dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode = code;

    if ((code = param_write_int(plist, "AdjustWidth", &fdev->AdjustWidth)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "MinFeatureSize", &fdev->MinFeatureSize)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "FillOrder", &fdev->FillOrder)) < 0)
        ecode = code;
     if ((code = param_write_bool(plist, "BlackIs1", &fdev->BlackIs1)) < 0)
        ecode = code;

    return ecode;
}
int
gdev_fax_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_fax *const fdev = (gx_device_fax *)dev;
    int ecode = 0;
    int code;
    int fill_order = fdev->FillOrder;
    bool blackis1 = fdev->BlackIs1;
    int aw = fdev->AdjustWidth;
    int mfs = fdev->MinFeatureSize;
    const char *param_name;

    switch (code = param_read_int(plist, (param_name = "AdjustWidth"), &aw)) {
        case 0:
            if (aw >= 0)
                break;
            code = gs_error_rangecheck;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }
    switch (code = param_read_int(plist, (param_name = "FillOrder"), &fill_order)) {
        case 0:
            if (fill_order == 1 || fill_order == 2)
                break;
            code = gs_error_rangecheck;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }
    switch (code = param_read_bool(plist, (param_name = "BlackIs1"), &blackis1)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }
    switch (code = param_read_int(plist, (param_name = "MinFeatureSize"), &mfs)) {
        case 0:
            if (mfs >= 0 && mfs <= 4)
                break;
            code = gs_error_rangecheck;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 1:
            break;
    }

    if (ecode < 0)
        return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        return code;

    fdev->AdjustWidth = aw;
    fdev->MinFeatureSize = mfs;
    fdev->FillOrder = fill_order;

    return code;
}

/* Initialize the stream state with a set of default parameters. */
/* These select the same defaults as the CCITTFaxEncode filter, */
/* except we set BlackIs1 = true. */
static void
gdev_fax_init_state_adjust(stream_CFE_state *ss,
                           const gx_device_fax *fdev,
                           int adjust_width)
{
    s_CFE_template.set_defaults((stream_state *)ss);
    ss->Columns = fdev->width;
    ss->Rows = fdev->height;
    ss->BlackIs1 = fdev->BlackIs1;
    ss->FirstBitLowOrder = fdev->FillOrder == 2;
    ss->Columns = fax_adjusted_width(ss->Columns, adjust_width);
}

void
gdev_fax_init_state(stream_CFE_state *ss, const gx_device_fax *fdev)
{
    gdev_fax_init_state_adjust(ss, fdev, 1);
}
void
gdev_fax_init_fax_state(stream_CFE_state *ss, const gx_device_fax *fdev)
{
    gdev_fax_init_state_adjust(ss, fdev, fdev->AdjustWidth);
}

/*
 * Print one strip with fax compression.  Fax devices call this once per
 * page; TIFF devices call this once per strip.
 */
int
gdev_fax_print_strip(gx_device_printer * pdev, gp_file * prn_stream,
                     const stream_template * temp, stream_state * ss,
                     int width, int row_first, int row_end /* last + 1 */)
{
    gs_memory_t *mem = pdev->memory;
    int code;
    stream_cursor_read r;
    stream_cursor_write w;
    int in_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    /*
     * Because of the width adjustment for fax systems, width may
     * be different from (either greater than or less than) pdev->width.
     * Allocate a large enough buffer to account for this.
     */
    int col_size = (width * pdev->color_info.depth + 7) >> 3;
    int max_size = max(in_size, col_size);
    int lnum = 0;
    int row_in = row_first;
    byte *in;
    byte *out;
    void *min_feature_data = NULL;
    /* If the file is 'nul', don't even do the writes. */
    bool nul = !strcmp(pdev->fname, "nul");
    int lnum_in = row_in;
    int min_feature_size = ((gx_device_fax *const)pdev)->MinFeatureSize;

    /* Initialize the common part of the encoder state. */
    ss->templat = temp;
    ss->memory = mem;
    /* Now initialize the encoder. */
    code = temp->init(ss);
    if (code < 0)
        return_error(gs_error_limitcheck);      /* bogus, but as good as any */

    /* Allocate the buffers. */
    in = gs_alloc_bytes(mem, temp->min_in_size + max_size + 1,
                        "gdev_stream_print_page(in)");
#define OUT_SIZE 1000
    out = gs_alloc_bytes(mem, OUT_SIZE, "gdev_stream_print_page(out)");
    if (in == 0 || out == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    /* Init the min_feature_size expansion for entire image (not strip) */
    /* This is only called from gdev_fax_print_page() and row_first is *always* 0
     * By removing this check, we can make it synonymous with the use of
     * min_feature_data below, so we don't need to check min_feature_data there.
     */
    if ((min_feature_size > 1) /* && (row_first == 0) */) {
        code = min_feature_size_init(mem, min_feature_size,
                                     width, pdev->height, &min_feature_data);
        if (code < 0)
            goto done;
    }
    if (min_feature_size > 1)
        row_in = max(0, row_first-min_feature_size);    /* need some data before this row */

    /* Set up the processing loop. */
    r.ptr = r.limit = in - 1;
    w.ptr = out - 1;
    w.limit = w.ptr + OUT_SIZE;
#undef OUT_SIZE

    /* Process the image. */
    for (lnum = row_in; ;) {
        int status;

        if_debug7m('w', mem,
                   "[w]lnum=%d r="PRI_INTPTR","PRI_INTPTR","PRI_INTPTR" w="PRI_INTPTR","PRI_INTPTR","PRI_INTPTR"\n", lnum,
                   (intptr_t)in, (intptr_t)r.ptr, (intptr_t)r.limit,
                   (intptr_t)out, (intptr_t)w.ptr, (intptr_t)w.limit);
        status = temp->process(ss, &r, &w, lnum == row_end);
        if_debug7m('w', mem,
                   "...%d, r="PRI_INTPTR","PRI_INTPTR","PRI_INTPTR" w="PRI_INTPTR","PRI_INTPTR","PRI_INTPTR"\n", status,
                   (intptr_t)in, (intptr_t)r.ptr, (intptr_t)r.limit,
                   (intptr_t)out, (intptr_t)w.ptr, (intptr_t)w.limit);
        switch (status) {
            case 0:             /* need more input data */
                if (lnum == row_end)
                    goto ok;
                {
                    uint left = r.limit - r.ptr;
                    int filtered_count = in_size;

                    memcpy(in, r.ptr + 1, left);
                    do {
                        if (lnum_in < row_end)
                        {
                            code = gdev_prn_copy_scan_lines(pdev, lnum_in++, in + left, in_size);
                            if (code < 0) {
                                code = gs_note_error(code);
                                goto done;
                            }
                        }
                        if (min_feature_size > 1)
                            filtered_count =
                                min_feature_size_process(in+left, min_feature_data);
                    } while (filtered_count == 0);
                    /* Note: we use col_size here, not in_size. */
                    lnum++;
                    if (col_size > in_size) {
                        memset(in + left + in_size, 0, col_size - in_size);
                    }
                    r.limit = in + left + col_size - 1;
                    r.ptr = in - 1;
                }
                break;
            case 1:             /* need to write output */
                if (!nul)
                    gp_fwrite(out, 1, w.ptr + 1 - out, prn_stream);
                w.ptr = out - 1;
                break;
        }
    }

  ok:
    /* Write out any remaining output. */
    if (!nul)
        gp_fwrite(out, 1, w.ptr + 1 - out, prn_stream);

  done:
    /* We only get one strip, we need to free min_feature_data without
     * any further checks or we will leak memory as we will allocate a
     * new one next time round. In truth it should only be possible to
     * get here with lnum != pdev-.Height in the case of an error, in
     * which case we still want to free the buffer!
     */
    if ((min_feature_size > 1) /* && (lnum == pdev->height) */)
        min_feature_size_dnit(min_feature_data);
    gs_free_object(mem, out, "gdev_stream_print_page(out)");
    gs_free_object(mem, in, "gdev_stream_print_page(in)");
    if (temp->release)
        temp->release(ss);
    return code;
}

/* Print a fax page.  Other fax drivers use this. */
int
gdev_fax_print_page(gx_device_printer * pdev, gp_file * prn_stream,
                    stream_CFE_state * ss)
{
    return gdev_fax_print_strip(pdev, prn_stream, &s_CFE_template,
                                (stream_state *)ss, ss->Columns,
                                0, pdev->height);
}

/* Print a 1-D Group 3 page. */
static int
faxg3_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)pdev);
    state.EndOfLine = true;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a 2-D Group 3 page. */
static int
faxg32d_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)pdev);
    state.K = (pdev->y_pixels_per_inch < 100 ? 2 : 4);
    state.EndOfLine = true;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a Group 4 page. */
static int
faxg4_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)pdev);
    state.K = -1;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}
