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


/* TIFF-writing substructure */

#include "stdint_.h"   /* for tiff.h */
#include "stdio_.h"
#include "time_.h"
#include "gdevtifs.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"
#include "minftrsz.h"
#include "gxdownscale.h"

#include <tiffio.h>
#include <tiffvers.h>

#ifdef __WIN32__
#define fseeko _fseeki64
#define ftello _ftelli64
typedef __int64 off_t;
#endif

#if defined(__MINGW32__) && __MINGW32__ == 1
#define ftello ftell
#define fseeko fseek
#endif

#define TIFF_PRINT_BUF_LENGTH 1024
static const char tifs_msg_truncated[] = "\n*** Previous line has been truncated.\n";

/* place to hold the data for our libtiff i/o hooks */
typedef struct tifs_io_private_t
{
    FILE *f;
    gx_device_printer *pdev;
} tifs_io_private;

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

    /* Use our own warning and error message handlers in libtiff */
    tiff_set_handlers();

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
#if (TIFFLIB_VERSION >= 20111221)
    if ((code = param_write_bool(plist, "UseBigTIFF", &tfdev->UseBigTIFF)) < 0)
        ecode = code;
#endif
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
    if ((code = param_write_long(plist, "AdjustWidth", &tfdev->AdjustWidth)) < 0)
        ecode = code;
    if ((code = param_write_long(plist, "MinFeatureSize", &tfdev->MinFeatureSize)) < 0)
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
    bool usebigtiff = tfdev->UseBigTIFF;
    uint16 compr = tfdev->Compression;
    gs_param_string comprstr;
    long downscale = tfdev->DownScaleFactor;
    long mss = tfdev->MaxStripSize;
    long aw = tfdev->AdjustWidth;
    long mfs = tfdev->MinFeatureSize;

    /* Read BigEndian option as bool */
    switch (code = param_read_bool(plist, (param_name = "BigEndian"), &big_endian)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

    /* Read UseBigTIFF option as bool */
    switch (code = param_read_bool(plist, (param_name = "UseBigTIFF"), &usebigtiff)) {
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        case 0:
        case 1:
            break;
    }

#if !(TIFFLIB_VERSION >= 20111221)
    if (usebigtiff)
        dmlprintf(dev->memory, "Warning: this version of libtiff does not support BigTIFF, ignoring parameter\n");
    usebigtiff = false;
#endif

    /* Read Compression */
    switch (code = param_read_string(plist, (param_name = "Compression"), &comprstr)) {
        case 0:
            if ((ecode = tiff_compression_id(&compr, &comprstr)) < 0 ||
                !tiff_compression_allowed(compr, (which & 1 ? 1 : dev->color_info.depth)))
            {
                errprintf(tfdev->memory,
                          (ecode < 0 ? "Unknown compression setting\n" :
                           "Invalid compression setting for this bitdepth\n"));
                param_signal_error(plist, param_name, ecode);
            }
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
    switch (code = param_read_long(plist, (param_name = "AdjustWidth"), &aw)) {
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
    switch (code = param_read_long(plist, (param_name = "MinFeatureSize"), &mfs)) {
        case 0:
            if ((mfs >= 0) && (mfs <= 4))
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
    tfdev->UseBigTIFF = usebigtiff;
    tfdev->Compression = compr;
    tfdev->MaxStripSize = mss;
    tfdev->DownScaleFactor = downscale;
    tfdev->AdjustWidth = aw;
    tfdev->MinFeatureSize = mfs;
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

/* libtiff i/o hooks */
static int
gs_tifsDummyMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
    (void) fd;
    (void) pbase;
    (void) psize;
    return (0);
}

static void
gs_tifsDummyUnmapProc(thandle_t fd, void* base, toff_t size)
{
    (void) fd;
    (void) base;
    (void) size;
}

static size_t
gs_tifsReadProc(thandle_t fd, void* buf, size_t size)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    size_t size_io = (size_t) size;

    if ((size_t) size_io != size) {
        return (size_t) -1;
    }
    return((size_t) fread (buf, 1, size_io, tiffio->f));
}

static size_t
gs_tifsWriteProc(thandle_t fd, void* buf, size_t size)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    size_t size_io = (size_t) size;
    size_t written;

    if ((size_t) size_io != size) {
        return (size_t) -1;
    }
    written = (size_t) fwrite (buf, 1, size_io, tiffio->f);
    return written;
}

static uint64_t
gs_tifsSeekProc(thandle_t fd, uint64_t off, int whence)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    off_t off_io = (off_t) off;

    if ((uint64_t) off_io != off) {
        return (uint64_t) -1; /* this is really gross */
    }
    if (fseeko(tiffio->f , off_io, whence) < 0) {
        return (uint64_t) -1;
    }
    return (ftello(tiffio->f));
}

static int
gs_tifsCloseProc(thandle_t fd)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    gx_device_printer *pdev = tiffio->pdev;
    int code = fclose(tiffio->f);
    
    gs_free(pdev->memory, tiffio, sizeof(tifs_io_private), 1, "gs_tifsCloseProc");

    return code;
}

static uint64_t
gs_tifsSizeProc(thandle_t fd)
{
    tifs_io_private *tiffio = (tifs_io_private *)fd;
    uint64_t length, curpos = (uint64_t)ftello(tiffio->f);
    
    if (curpos < 0) {
        return(0);
    }
    
    if (fseeko(tiffio->f, 0, SEEK_END) < 0) {
        return(0);
    }
    length = (uint64_t)ftello(tiffio->f);
    
    if (fseeko(tiffio->f, curpos, SEEK_SET) < 0) {
        return(0);
    }
    return length;
}

TIFF *
tiff_from_filep(gx_device_printer *dev,  const char *name, FILE *filep, int big_endian, bool usebigtiff)
{
    int fd;
    char mode[5] = "w";
    int modelen = 1;
    TIFF *t;
    tifs_io_private *tiffio;

#ifdef __WIN32__
        fd = _get_osfhandle(fileno(filep));
#else
        fd = fileno(filep);
#endif

    if (fd < 0)
        return NULL;

    if (big_endian)
        mode[modelen++] = 'b';
    else
        mode[modelen++] = 'l';

    if (usebigtiff)
    /* this should never happen for libtiff < 4.0 - see tiff_put_some_params() */
        mode[modelen++] = '8';

    mode[modelen++] = (char)0;
    
    tiffio = (tifs_io_private *)gs_malloc(dev->memory, sizeof(tifs_io_private), 1, "tiff_from_filep");
    if (!tiffio) {
        return NULL;
    }
    tiffio->f = filep;
    tiffio->pdev = dev;

    t = TIFFClientOpen(name, mode,
        (thandle_t) tiffio, (TIFFReadWriteProc)gs_tifsReadProc,
        (TIFFReadWriteProc)gs_tifsWriteProc, (TIFFSeekProc)gs_tifsSeekProc,
	gs_tifsCloseProc, (TIFFSizeProc)gs_tifsSizeProc, gs_tifsDummyMapProc,
        gs_tifsDummyUnmapProc);

    return t;
}

int gdev_tiff_begin_page(gx_device_tiff *tfdev,
                         FILE *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)tfdev;

    if (gdev_prn_file_is_new(pdev)) {
        /* open the TIFF device */
        tfdev->tif = tiff_from_filep(pdev, pdev->dname, file, tfdev->BigEndian, tfdev->UseBigTIFF);
        if (!tfdev->tif)
            return_error(gs_error_invalidfileaccess);
    }

    return tiff_set_fields_for_printer(pdev, tfdev->tif, tfdev->DownScaleFactor,
                                       tfdev->AdjustWidth);
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
                                int                factor,
                                int                adjustWidth)
{
    int width = gx_downscaler_scale(pdev->width, factor);
    int height = gx_downscaler_scale(pdev->height, factor);
    int xpi = gx_downscaler_scale(pdev->x_pixels_per_inch, factor);
    int ypi = gx_downscaler_scale(pdev->y_pixels_per_inch, factor);
    width = fax_adjusted_width(width, adjustWidth);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, (float)xpi);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, (float)ypi);

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

    /* Set the ICC profile.  Test to avoid issues with separations and also
       if the color space is set to LAB, we do that as an enumerated type */
    if (pdev->icc_struct != NULL && pdev->icc_struct->device_profile[0] != NULL) {
        cmm_profile_t *icc_profile = pdev->icc_struct->device_profile[0];
        if (icc_profile->num_comps == pdev->color_info.num_components &&
            icc_profile->data_cs != gsCIELAB) {
            TIFFSetField(tif, TIFFTAG_ICCPROFILE, icc_profile->buffer_size, 
                         icc_profile->buffer);
        }
    }
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

    code = TIFFCheckpointDirectory(tif);

    memset(data, 0, max_size);
    for (row = 0; row < dev->height && code >= 0; row++) {
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

            code = TIFFWriteScanline(tif, data, row - line_lag, 0);
        }
    }
    for (row -= line_lag ; row < dev->height && code >= 0; row++)
    {
        filtered_count = min_feature_size_process(data, min_feature_data);
        code = TIFFWriteScanline(tif, data, row, 0);
    }

    if (code >= 0)
        code = TIFFWriteDirectory(tif);
cleanup:
    if (min_feature_size > 1)
        min_feature_size_dnit(min_feature_data);
    gs_free_object(dev->memory, data, "tiff_print_page(data)");

    return code;
}

/* Special version, called with 8 bit grey input to be downsampled to 1bpp
 * output. */
int
tiff_downscale_and_print_page(gx_device_printer *dev, TIFF *tif, int factor,
                              int mfs, int aw, int bpc, int num_comps)
{
    int code = 0;
    byte *data = NULL;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = max(size, TIFFScanlineSize(tif));
    int row;
    int height = dev->height/factor;
    gx_downscaler_t ds;

    code = TIFFCheckpointDirectory(tif);
    if (code < 0)
        return code;

    code = gx_downscaler_init(&ds, (gx_device *)dev, 8, bpc, num_comps,
                              factor, mfs, &fax_adjusted_width, aw);
    if (code < 0)
        return code;

    data = gs_alloc_bytes(dev->memory, max_size, "tiff_print_page(data)");
    if (data == NULL) {
        gx_downscaler_fin(&ds);
        return_error(gs_error_VMerror);
    }

    for (row = 0; row < height && code >= 0; row++) {
        code = gx_downscaler_copy_scan_lines(&ds, row, data, size);
        if (code < 0)
            break;

        code = TIFFWriteScanline(tif, data, row, 0);
        if (code < 0)
            break;
    }

    if (code >= 0)
        code = TIFFWriteDirectory(tif);

    gx_downscaler_fin(&ds);
    gs_free_object(dev->memory, data, "tiff_print_page(data)");

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

static void
gs_tifsWarningHandlerEx(thandle_t client_data, const char* module, const char* fmt, va_list ap)
{
    tifs_io_private *tiffio = (tifs_io_private *)client_data;
    gx_device_printer *pdev = tiffio->pdev;    
    int count;
    char buf[TIFF_PRINT_BUF_LENGTH];

    count = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (count >= sizeof(buf) || count < 0)  { /* C99 || MSVC */
        dmlprintf1(pdev->memory, "%s", buf);
        dmlprintf1(pdev->memory, "%s\n", tifs_msg_truncated);
    } else {
        dmlprintf1(pdev->memory, "%s\n", buf);
    }
}

static void
gs_tifsErrorHandlerEx(thandle_t client_data, const char* module, const char* fmt, va_list ap)
{
    tifs_io_private *tiffio = (tifs_io_private *)client_data;
    gx_device_printer *pdev = tiffio->pdev;    
    const char *max_size_error = "Maximum TIFF file size exceeded";
    int count;
    char buf[TIFF_PRINT_BUF_LENGTH];

    count = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (count >= sizeof(buf) || count < 0)  { /* C99 || MSVC */
        dmlprintf1(pdev->memory, "%s\n", buf);
        dmlprintf1(pdev->memory, "%s", tifs_msg_truncated);
    } else {
        dmlprintf1(pdev->memory, "%s\n", buf);
    }

#if (TIFFLIB_VERSION >= 20111221)
    if (!strncmp(fmt, max_size_error, strlen(max_size_error))) {
        dmlprintf(pdev->memory, "Use -dUseBigTIFF(=true) for BigTIFF output\n");
    }
#endif
}

void tiff_set_handlers (void)
{
    (void)TIFFSetErrorHandler(NULL);
    (void)TIFFSetWarningHandler(NULL);
    (void)TIFFSetErrorHandlerExt(gs_tifsErrorHandlerEx);
    (void)TIFFSetWarningHandlerExt(gs_tifsWarningHandlerEx);
}
