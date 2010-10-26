/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id$ */
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
const gx_device_procs gdev_fax_std_procs =
    prn_params_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
                     gdev_fax_get_params, gdev_fax_put_params);

#define FAX_DEVICE(dname, print_page)\
{\
    FAX_DEVICE_BODY(gx_device_fax, gdev_fax_std_procs, dname, print_page)\
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
    return ecode;
}
int
gdev_fax_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_fax *const fdev = (gx_device_fax *)dev;
    int ecode = 0;
    int code;
    int aw = fdev->AdjustWidth;
    int mfs = fdev->MinFeatureSize;
    const char *param_name;

    switch (code = param_read_int(plist, (param_name = "AdjustWidth"), &aw)) {
        case 0:
            if (aw >= 0 && aw <= 1)
                break;
            code = gs_error_rangecheck;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
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
    return code;
}

int
gdev_fax_adjusted_width(int width)
{
    /* Adjust the page width to a legal value for fax systems. */
    if (width >= 1680 && width <= 1736)
        /* Adjust width for A4 paper. */
        return 1728;
    else if (width >= 2000 && width <= 2056)
        /* Adjust width for B4 paper. */
        return 2048;
    else
        return width;
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
    ss->BlackIs1 = true;
    if (adjust_width > 0)
        ss->Columns = gdev_fax_adjusted_width(ss->Columns);
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
gdev_fax_print_strip(gx_device_printer * pdev, FILE * prn_stream,
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
    ss->template = temp;
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
    if ((min_feature_size > 1) && (row_first == 0))
        code = min_feature_size_init(mem, min_feature_size,
                                     width, pdev->height, &min_feature_data);
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

        if_debug7('w', "[w]lnum=%d r=0x%lx,0x%lx,0x%lx w=0x%lx,0x%lx,0x%lx\n", lnum,
                  (ulong)in, (ulong)r.ptr, (ulong)r.limit,
                  (ulong)out, (ulong)w.ptr, (ulong)w.limit);
        status = temp->process(ss, &r, &w, lnum == row_end);
        if_debug7('w', "...%d, r=0x%lx,0x%lx,0x%lx w=0x%lx,0x%lx,0x%lx\n", status,
                  (ulong)in, (ulong)r.ptr, (ulong)r.limit,
                  (ulong)out, (ulong)w.ptr, (ulong)w.limit);
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
                                gs_note_error(code);
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
                    fwrite(out, 1, w.ptr + 1 - out, prn_stream);
                w.ptr = out - 1;
                break;
        }
    }

  ok:
    /* Write out any remaining output. */
    if (!nul)
        fwrite(out, 1, w.ptr + 1 - out, prn_stream);

  done:
    if ((min_feature_size > 1) && (lnum == pdev->height))
        min_feature_size_dnit(min_feature_data);
    gs_free_object(mem, out, "gdev_stream_print_page(out)");
    gs_free_object(mem, in, "gdev_stream_print_page(in)");
    if (temp->release)
        temp->release(ss);
    return code;
}

/* Print a fax page.  Other fax drivers use this. */
int
gdev_fax_print_page(gx_device_printer * pdev, FILE * prn_stream,
                    stream_CFE_state * ss)
{
    return gdev_fax_print_strip(pdev, prn_stream, &s_CFE_template,
                                (stream_state *)ss, ss->Columns,
                                0, pdev->height);
}

/* Print a 1-D Group 3 page. */
static int
faxg3_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)pdev);
    state.EndOfLine = true;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}

/* Print a 2-D Group 3 page. */
static int
faxg32d_print_page(gx_device_printer * pdev, FILE * prn_stream)
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
faxg4_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    stream_CFE_state state;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)pdev);
    state.K = -1;
    state.EndOfBlock = false;
    return gdev_fax_print_page(pdev, prn_stream, &state);
}
