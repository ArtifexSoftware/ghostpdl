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
#include "gdevtifs.h"
#include "stdio_.h"
#include "time_.h"
#include "gstypes.h"
#include "gscdefs.h"
#include "gdevprn.h"

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

int
tiff_get_params(gx_device * dev, gs_param_list * plist)
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
    if ((code = param_write_long(plist, "MaxStripSize", &tfdev->MaxStripSize)) < 0)
        ecode = code;
    return ecode;
}

int
tiff_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tiff *const tfdev = (gx_device_tiff *)dev;
    int ecode = 0;
    int code;
    const char *param_name;
    bool big_endian = tfdev->BigEndian;
    uint16 compr = tfdev->Compression;
    gs_param_string comprstr;
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
    return code;
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

    return tiff_set_fields_for_printer(pdev, tfdev->tif);
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

int tiff_set_fields_for_printer(gx_device_printer *pdev, TIFF *tif)
{
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, pdev->width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, pdev->height);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, pdev->x_pixels_per_inch);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, pdev->y_pixels_per_inch);

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
tiff_print_page(gx_device_printer *dev, TIFF *tif)
{
    int code;
    byte *data;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = max(size, TIFFScanlineSize(tif));
    int row;
    int bpc = dev->color_info.depth / dev->color_info.num_components;

    data = gs_alloc_bytes(dev->memory, max_size, "tiff_print_page(data)");
    if (data == NULL)
	return_error(gs_error_VMerror);

    memset(data, 0, max_size);
    for (row = 0; row < dev->height; row++) {
	code = gdev_prn_copy_scan_lines(dev, row, data, size);
	if (code < 0)
	    break;

#if defined(ARCH_IS_BIG_ENDIAN) && (!ARCH_IS_BIG_ENDIAN) 
	if (bpc == 16)
	    TIFFSwabArrayOfShort((uint16 *)data,
				 dev->width * dev->color_info.num_components);
#endif

	TIFFWriteScanline(tif, data, row, 0);
    }

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

