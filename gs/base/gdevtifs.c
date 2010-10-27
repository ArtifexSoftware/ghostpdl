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
/* TIFF-writing substructure */

#include "stdint_.h"   /* for tiff.h */
#include "stdio_.h"
#include "time_.h"
#include "gdevtifs.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"
#include "minftrsz.h"

#include <tiffio.h>

/*
 * Open the output seekable, because libtiff doesn't support writing to
 * non-positionable streams. Otherwise, these are the same as
 * gdev_prn_output_page() and gdev_prn_open().
 */
int
tiff_output_page(gx_device *pdev, int num_copies, int flush)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int outcode = 0, closecode = 0, errcode = 0, endcode;
    bool upgraded_copypage = false;

    if (num_copies > 0 || !flush) {
        int code = gdev_prn_open_printer_positionable(pdev, 1, 1);

        if (code < 0)
            return code;

        /* If copypage request, try to do it using buffer_page */
        if ( !flush &&
             (*ppdev->printer_procs.buffer_page)
             (ppdev, ppdev->file, num_copies) >= 0
             ) {
            upgraded_copypage = true;
            flush = true;
        }
        else if (num_copies > 0)
            /* Print the accumulated page description. */
            outcode =
                (*ppdev->printer_procs.print_page_copies)(ppdev, ppdev->file,
                                                          num_copies);
        fflush(ppdev->file);
        errcode =
            (ferror(ppdev->file) ? gs_note_error(gs_error_ioerror) : 0);
        if (!upgraded_copypage)
            closecode = gdev_prn_close_printer(pdev);
    }
    endcode = (ppdev->buffer_space && !ppdev->is_async_renderer ?
               clist_finish_page(pdev, flush) : 0);

    if (outcode < 0)
        return outcode;
    if (errcode < 0)
        return errcode;
    if (closecode < 0)
        return closecode;
    if (endcode < 0)
        return endcode;
    endcode = gx_finish_output_page(pdev, num_copies, flush);
    return (endcode < 0 ? endcode : upgraded_copypage ? 1 : 0);
}

int
tiff_open(gx_device *pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code;

    ppdev->file = NULL;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (code < 0)
        return code;
    if (ppdev->OpenOutputFile)
        code = gdev_prn_open_printer_seekable(pdev, 1, true);
    return code;
}

int
tiff_close(gx_device * pdev)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)pdev;

    if (tfdev->tif)
        TIFFCleanup(tfdev->tif);

    return gdev_prn_close(pdev);
}

static int
tiff_get_some_params(gx_device * dev, gs_param_list * plist, int which)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode = code;
    gs_param_string comprstr;

    if ((code = param_write_bool(plist, "BigEndian", &tfdev->BigEndian)) < 0)
        ecode = code;
    if ((code = tiff_compression_param_string(&comprstr, tfdev->Compression)) < 0 ||
        (code = param_write_string(plist, "Compression", &comprstr)) < 0)
        ecode = code;
    if (which & 1)
    {
      if ((code = param_write_long(plist, "DownScaleFactor", &tfdev->DownScaleFactor)) < 0)
          ecode = code;
    }
    if ((code = param_write_long(plist, "MaxStripSize", &tfdev->MaxStripSize)) < 0)
        ecode = code;
    return ecode;
}

int
tiff_get_params(gx_device * dev, gs_param_list * plist)
{
    return tiff_get_some_params(dev, plist, 0);
}

int
tiff_get_params_downscale(gx_device * dev, gs_param_list * plist)
{
    return tiff_get_some_params(dev, plist, 1);
}

static int
tiff_put_some_params(gx_device * dev, gs_param_list * plist, int which)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int ecode = 0;
    int code;
    const char *param_name;
    bool big_endian = tfdev->BigEndian;
    uint16 compr = tfdev->Compression;
    gs_param_string comprstr;
    long downscale = tfdev->DownScaleFactor;
    long mss = tfdev->MaxStripSize;

    /* Read BigEndian option as bool */
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &big_endian)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }
    /* Read Compression */
    switch (code = param_read_string(plist, (param_name = "Compression"), &comprstr)) {
        case 0:
            if ((ecode = tiff_compression_id(&compr, &comprstr)) < 0 ||
                !tiff_compression_allowed(compr, dev->color_info.depth))
                param_signal_error(plist, param_name, ecode);
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }
    /* Read Downscale factor */
    if (which & 1) {
        switch (code = param_read_long(plist,
                                       (param_name = "DownScaleFactor"),
                                       &downscale)) {
            case 0:
                if (downscale <= 0)
                    downscale = 1;
                break;
            case 1:
                break;
            default:
                ecode = code;
                param_signal_error(plist, param_name, ecode);
        }
    }
    switch (code = param_read_long(plist, (param_name = "MaxStripSize"), &mss)) {
        case 0:
            /*
             * Strip must be large enough to accommodate a raster line.
             * If the max strip size is too small, we still write a single
             * line per strip rather than giving an error.
             */
            if (mss >= 0)
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

    tfdev->BigEndian = big_endian;
    tfdev->Compression = compr;
    tfdev->MaxStripSize = mss;
    tfdev->DownScaleFactor = downscale;
    return code;
}

int
tiff_put_params(gx_device * dev, gs_param_list * plist)
{
    return tiff_put_some_params(dev, plist, 0);
}

int
tiff_put_params_downscale(gx_device * dev, gs_param_list * plist)
{
    return tiff_put_some_params(dev, plist, 1);
}

TIFF *
tiff_from_filep(const char *name, FILE *filep, int big_endian)
{
    int fd;

#ifdef __WIN32__
        fd = _get_osfhandle(fileno(filep));
#else
        fd = fileno(filep);
#endif

    if (fd < 0)
        return NULL;

    return TIFFFdOpen(fd, name, big_endian ? "wb" : "wl");
}

int gdev_tiff_begin_page(gx_device_tiff *tfdev,
                         FILE *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)tfdev;

    if (gdev_prn_file_is_new(pdev)) {
        /* open the TIFF device */
        tfdev->tif = tiff_from_filep(pdev->dname, file, tfdev->BigEndian);
        if (!tfdev->tif)
            return_error(gs_error_invalidfileaccess);
    }

    return tiff_set_fields_for_printer(pdev, tfdev->tif, tfdev->DownScaleFactor);
}

int tiff_set_compression(gx_device_printer *pdev,
                         TIFF *tif,
                         uint compression,
                         long max_strip_size)
{
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    if (max_strip_size == 0) {
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, pdev->height);
    }
    else {
        int rows = max_strip_size /
            gdev_mem_bytes_per_scan_line((gx_device *)pdev);
        TIFFSetField(tif,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(tif, max(1, rows)));
    }

    return 0;
}

int tiff_set_fields_for_printer(gx_device_printer *pdev,
                                TIFF              *tif,
                                int                factor)
{
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (pdev->width + factor-1)/factor);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (pdev->height + factor-1)/factor);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)pdev->x_pixels_per_inch/factor);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)pdev->y_pixels_per_inch/factor);

    {
        char revs[10];
#define maxSoftware 40
        char softwareValue[maxSoftware];

        strncpy(softwareValue, gs_product, maxSoftware);
        softwareValue[maxSoftware - 1] = 0;
        sprintf(revs, " %1.2f", gs_revision / 100.0);
        strncat(softwareValue, revs,
                maxSoftware - strlen(softwareValue) - 1);

        TIFFSetField(tif, TIFFTAG_SOFTWARE, softwareValue);
    }
    {
        struct tm tms;
        time_t t;
        char dateTimeValue[20];

        time(&t);
        tms = *localtime(&t);
        sprintf(dateTimeValue, "%04d:%02d:%02d %02d:%02d:%02d",
                tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                tms.tm_hour, tms.tm_min, tms.tm_sec);

        TIFFSetField(tif, TIFFTAG_DATETIME, dateTimeValue);
    }

    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_PAGENUMBER, pdev->PageCount, 0);

    return 0;
}

int
tiff_print_page(gx_device_printer *dev, TIFF *tif, int min_feature_size)
{
    int code = 0;
    byte *data;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = max(size, TIFFScanlineSize(tif));
    int row;
    int bpc = dev->color_info.depth / dev->color_info.num_components;
    void *min_feature_data = NULL;
    int line_lag = 0;
    int filtered_count;

    data = gs_alloc_bytes(dev->memory, max_size, "tiff_print_page(data)");
    if (data == NULL)
        return_error(gs_error_VMerror);
    if (bpc != 1)
        min_feature_size = 1;
    if (min_feature_size > 1) {
        code = min_feature_size_init(dev->memory, min_feature_size,
                                     dev->width, dev->height,
                                     &min_feature_data);
        if (code < 0)
            goto cleanup;
    }

    TIFFCheckpointDirectory(tif);

    memset(data, 0, max_size);
    for (row = 0; row < dev->height; row++) {
        code = gdev_prn_copy_scan_lines(dev, row, data, size);
        if (code < 0)
            break;
        if (min_feature_size > 1) {
            filtered_count = min_feature_size_process(data, min_feature_data);
            if (filtered_count == 0)
                line_lag++;
        }

        if (row - line_lag >= 0) {
#if defined(ARCH_IS_BIG_ENDIAN) && (!ARCH_IS_BIG_ENDIAN)
            if (bpc == 16)
                TIFFSwabArrayOfShort((uint16 *)data,
                                     dev->width * dev->color_info.num_components);
#endif

            TIFFWriteScanline(tif, data, row - line_lag, 0);
        }
    }
    for (row -= line_lag ; row < dev->height; row++)
    {
        filtered_count = min_feature_size_process(data, min_feature_data);
        TIFFWriteScanline(tif, data, row, 0);
    }

    TIFFWriteDirectory(tif);
cleanup:
    if (min_feature_size > 1)
        min_feature_size_dnit(min_feature_data);
    gs_free_object(dev->memory, data, "tiff_print_page(data)");

    return code;
}

/* Error diffusion data is stored in errors block.
 * We have 1 empty entry at each end to avoid overflow. When
 * moving left to right we read from entries 2->width+1 (inclusive), and
 * write to 1->width. When moving right to left we read from width->1 and
 * write to width+1->2.
 */
static void down_and_out(TIFF *tif,
                         byte *data,
                         int   max_size,
                         int   factor,
                         int   row,
                         int   width,
                         int  *errors)
{
    int x, xx, y, value;
    byte *outp;
    byte *inp = data;
    int mask;
    int e_downleft, e_down, e_forward = 0;
    const int threshold = factor*factor*128;
    const int max_value = factor*factor*255;

    if ((row & 1) == 0)
    {
        /* Left to Right pass */
        const int back = max_size * factor -1;
        errors += 2;
        outp = inp;
        for (x = width; x > 0; x--)
        {
            value = e_forward + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += max_size;
                }
                inp -= back;
            }
            if (value >= threshold)
            {
                *outp++ = 1;
                value -= max_value;
            }
            else
            {
                *outp++ = 0;
            }
            e_forward  = value * 7/16;
            e_downleft = value * 3/16;
            e_down     = value * 5/16;
            value     -= e_forward + e_downleft + e_down;
            errors[-2] += e_downleft;
            errors[-1] += e_down;
            *errors++   = value;
        }
        outp -= width;
    }
    else
    {
        /* Right to Left pass */
        const int back = max_size * factor + 1;
        errors += width;
        inp += width*factor-1;
        outp = inp;
        for (x = width; x > 0; x--)
        {
            value = e_forward + *errors;
            for (xx = factor; xx > 0; xx--)
            {
                for (y = factor; y > 0; y--)
                {
                    value += *inp;
                    inp += max_size;
                }
                inp -= back;
            }
            if (value >= threshold)
            {
                *outp-- = 1;
                value -= max_value;
            }
            else
            {
                *outp-- = 0;
            }
            e_forward  = value * 7/16;
            e_downleft = value * 3/16;
            e_down     = value * 5/16;
            value     -= e_forward + e_downleft + e_down;
            errors[2] += e_downleft;
            errors[1] += e_down;
            *errors--   = value;
        }
        outp++;
    }
    /* Now pack the data pointed to by outp into byte form */
    data = inp = outp;
    outp--;
    mask = 0;
    for (x = width; x > 0; x--)
    {
        value = *inp++;
        if (mask == 0)
        {
            mask = 128;
            *++outp = 0;
        }
        if (value)
            *outp |= mask;
        mask >>= 1;
    }

    TIFFWriteScanline(tif, data, row, 0);
}

/* Special version, called with 8 bit grey input to be downsampled to 1bpp
 * output. */
int
tiff_downscale_and_print_page(gx_device_printer *dev, TIFF *tif, int factor)
{
    int code = 0;
    byte *data;
    int *errors;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = max(size, TIFFScanlineSize(tif)) + factor-1;
    int row;
    int n;
    int width = (dev->width + factor-1)/factor;

    data = gs_alloc_bytes(dev->memory,
                          max_size * factor,
                          "tiff_print_page(data)");
    if (data == NULL)
        return_error(gs_error_VMerror);

    errors = (int *)gs_alloc_bytes(dev->memory,
                                   (width+3) * sizeof(int),
                                   "tiff_print_page(errors)");
    if (errors == NULL)
    {
        gs_free_object(dev->memory, data, "tiff_print_page(data)");
        return_error(gs_error_VMerror);
    }

    TIFFCheckpointDirectory(tif);

    memset(data, 0, max_size * factor);
    memset(errors, 0, (width+3) * sizeof(int));
    n = 0;
    for (row = 0; row < dev->height; row++) {
        code = gdev_prn_copy_scan_lines(dev, row, data + max_size*n, size);
        if (code < 0)
            break;
        n++;
        if (n == factor)
        {
            /* Do the downsample */
            n = 0;
            down_and_out(tif, data, max_size, factor, row/factor, width, errors);
        }
    }
    if (n != 0)
    {
        row--;
        while (n != factor)
        {
            memset(data + max_size * n, 0, max_size);
            n++;
            row++;
        }
        down_and_out(tif, data, max_size, factor, row/factor, width, errors);
    }

    gs_free_object(dev->memory, errors, "tiff_print_page(errors)");
    gs_free_object(dev->memory, data, "tiff_print_page(data)");

    TIFFWriteDirectory(tif);
    return code;
}


static struct compression_string {
    uint16 id;
    const char *str;
} compression_strings [] = {
    { COMPRESSION_NONE, "none" },
    { COMPRESSION_CCITTRLE, "crle" },
    { COMPRESSION_CCITTFAX3, "g3" },
    { COMPRESSION_CCITTFAX4, "g4" },
    { COMPRESSION_LZW, "lzw" },
    { COMPRESSION_PACKBITS, "pack" },

    { 0, NULL }
};

int
tiff_compression_param_string(gs_param_string *param, uint16 id)
{
    struct compression_string *c;
    for (c = compression_strings; c->str; c++)
        if (id == c->id) {
            param_string_from_string(*param, c->str);
            return 0;
        }
    return gs_error_undefined;
}

int
tiff_compression_id(uint16 *id, gs_param_string *param)
{
    struct compression_string *c;
    for (c = compression_strings; c->str; c++)
        if (!bytes_compare(param->data, param->size,
                           (const byte *)c->str, strlen(c->str)))
        {
            *id = c->id;
            return 0;
        }
    return gs_error_undefined;
}

int tiff_compression_allowed(uint16 compression, byte depth)
{
    return depth == 1 || (compression != COMPRESSION_CCITTRLE &&
                          compression != COMPRESSION_CCITTFAX3 &&
                          compression != COMPRESSION_CCITTFAX4);

}

