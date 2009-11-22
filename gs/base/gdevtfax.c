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
/* TIFF and TIFF/fax devices */
#include "gdevtifs.h"
#include "gdevprn.h"
#include "strimpl.h"
#include "scfx.h"
#include "gdevfax.h"

/* ---------------- TIFF/fax output ---------------- */

/* The device descriptors */

static dev_proc_open_device(tfax_open);
static dev_proc_close_device(tfax_close);
static dev_proc_get_params(tfax_get_params);
static dev_proc_put_params(tfax_put_params);
static dev_proc_print_page(tiffcrle_print_page);
static dev_proc_print_page(tiffg3_print_page);
static dev_proc_print_page(tiffg32d_print_page);
static dev_proc_print_page(tiffg4_print_page);

struct gx_device_tfax_s {
    gx_device_common;
    gx_prn_device_common;
    gx_fax_device_common;
    long MaxStripSize;		/* 0 = no limit, other is UNCOMPRESSED limit */
                                /* The type and range of FillOrder follows TIFF 6 spec  */
    int  FillOrder;             /* 1 = lowest column in the high-order bit, 2 = reverse */
    bool  BigEndian;            /* true = big endian; false = little endian*/
    uint16 Compression;		/* same values as TIFFTAG_COMPRESSION */

    TIFF *tif;			/* For TIFF output only */
};
typedef struct gx_device_tfax_s gx_device_tfax;

/* Define procedures that adjust the paper size. */
static const gx_device_procs gdev_tfax_std_procs =
    prn_params_procs(tfax_open, tiff_output_page, tfax_close,
		     tfax_get_params, tfax_put_params);

#define TFAX_DEVICE(dname, print_page, compr)\
{\
    FAX_DEVICE_BODY(gx_device_tfax, gdev_tfax_std_procs, dname, print_page),\
    0				/* unlimited strip size byte count */,\
    1                           /* lowest column in the high-order bit */,\
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,\
    compr,\
    NULL\
}

const gx_device_tfax gs_tiffcrle_device =
    TFAX_DEVICE("tiffcrle", tiffcrle_print_page, COMPRESSION_CCITTRLE);

const gx_device_tfax gs_tiffg3_device =
    TFAX_DEVICE("tiffg3", tiffg3_print_page, COMPRESSION_CCITTFAX3);

const gx_device_tfax gs_tiffg32d_device =
    TFAX_DEVICE("tiffg32d", tiffg32d_print_page, COMPRESSION_CCITTFAX3);

const gx_device_tfax gs_tiffg4_device =
    TFAX_DEVICE("tiffg4", tiffg4_print_page, COMPRESSION_CCITTFAX4);


static int
tfax_open(gx_device * pdev)
{
    gx_device_printer * const ppdev = (gx_device_printer *)pdev;
    int code;

    ppdev->file = NULL;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (code < 0)
	return code;

    if (ppdev->OpenOutputFile)
	if ((code = gdev_prn_open_printer_seekable(pdev, 1, true)) < 0)
	    return code;

    return code;
}

static int
tfax_close(gx_device * pdev)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)pdev;

    TIFFCleanup(tfdev->tif);

    return gdev_prn_close(pdev);
}

static int
tfax_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int code = gdev_fax_get_params(dev, plist);
    int ecode = code;
    gs_param_string comprstr;


    if ((code = param_write_long(plist, "MaxStripSize", &tfdev->MaxStripSize)) < 0)
        ecode = code;
    if ((code = param_write_int(plist, "FillOrder", &tfdev->FillOrder)) < 0)
        ecode = code;
    if ((code = param_write_bool(plist, "BigEndian", &tfdev->BigEndian)) < 0)
        ecode = code;
    if ((code = tiff_compression_param_string(&comprstr, tfdev->Compression)) < 0 ||
	(code = param_write_string(plist, "Compression", &comprstr)) < 0)
	ecode = code;

    return ecode;
}

static int
tfax_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;
    int ecode = 0;
    int code;
    long mss = tfdev->MaxStripSize;
    int fill_order = tfdev->FillOrder;
    const char *param_name;
    bool big_endian = tfdev->BigEndian;
    uint16 compr = tfdev->Compression;
    gs_param_string comprstr;

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

    /* Following TIFF spec, FillOrder is integer */ 
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

    if (ecode < 0)
	return ecode;
    code = gdev_fax_put_params(dev, plist);
    if (code < 0)
	return code;

    tfdev->MaxStripSize = mss;
    tfdev->FillOrder = fill_order;
    tfdev->BigEndian = big_endian;
    tfdev->Compression = compr;
    return code;
}


/* ---------------- Other TIFF output ---------------- */

#include "slzwx.h"
#include "srlx.h"

/* Device descriptors for TIFF formats other than fax. */
static dev_proc_print_page(tifflzw_print_page);
static dev_proc_print_page(tiffpack_print_page);

const gx_device_tfax gs_tifflzw_device = {
    prn_device_std_body(gx_device_tfax, gdev_tfax_std_procs, "tifflzw",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* margins */
			1, tifflzw_print_page),
    1				/* AdjustWidth, not used */,
    0				/* unlimited strip size byte count */,
    1                           /* lowest column in the high-order bit, not used */,
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_LZW
};

const gx_device_tfax gs_tiffpack_device = {
    prn_device_std_body(gx_device_tfax, gdev_tfax_std_procs, "tiffpack",
			DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
			X_DPI, Y_DPI,
			0, 0, 0, 0,	/* margins */
			1, tiffpack_print_page),
    1				/* AdjustWidth, not used */,
    0				/* unlimited strip size byte count */,
    1                           /* lowest column in the high-order bit, not used */,
    arch_is_big_endian          /* default to native endian (i.e. use big endian iff the platform is so*/,
    COMPRESSION_PACKBITS
};


/* Forward references */
static int tfax_begin_page(gx_device_tfax * tfdev, FILE * file, int width);

static void
tfax_set_fields(gx_device_tfax *tfdev)
{
    short fillorder = tfdev->FillOrder == 1 ? FILLORDER_MSB2LSB : FILLORDER_LSB2MSB;

    TIFFSetField(tfdev->tif, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(tfdev->tif, TIFFTAG_COMPRESSION, tfdev->Compression);
    TIFFSetField(tfdev->tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(tfdev->tif, TIFFTAG_FILLORDER, fillorder);
    TIFFSetField(tfdev->tif, TIFFTAG_SAMPLESPERPIXEL, 1);
}

static int
tiffcrle_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *tfdev = (gx_device_tfax *)dev;

    tfax_begin_page(tfdev, prn_stream, gdev_fax_adjusted_width(dev->width));

    tfax_set_fields(tfdev);

    return tiff_print_page(dev, tfdev->tif);
}

static int
tiffg3_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *tfdev = (gx_device_tfax *)dev;

    tfax_begin_page(tfdev, prn_stream, gdev_fax_adjusted_width(dev->width));

    tfax_set_fields(tfdev);
    if (tfdev->Compression == COMPRESSION_CCITTFAX3)
	TIFFSetField(tfdev->tif, TIFFTAG_GROUP3OPTIONS, GROUP3OPT_FILLBITS);

    return tiff_print_page(dev, tfdev->tif);
}

static int
tiffg32d_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *tfdev = (gx_device_tfax *)dev;

    tfax_begin_page(tfdev, prn_stream, gdev_fax_adjusted_width(dev->width));

    tfax_set_fields(tfdev);
    if (tfdev->Compression == COMPRESSION_CCITTFAX3)
	TIFFSetField(tfdev->tif, TIFFTAG_GROUP3OPTIONS, GROUP3OPT_2DENCODING | GROUP3OPT_FILLBITS);

    return tiff_print_page(dev, tfdev->tif);
}

static int
tiffg4_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *tfdev = (gx_device_tfax *)dev;

    tfax_begin_page(tfdev, prn_stream, gdev_fax_adjusted_width(dev->width));

    tfax_set_fields(tfdev);
    if (tfdev->Compression == COMPRESSION_CCITTFAX4)
	TIFFSetField(tfdev->tif, TIFFTAG_GROUP4OPTIONS, 0);

    return tiff_print_page(dev, tfdev->tif);
}

/* Print an LZW page. */
static int
tifflzw_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;

    tfax_begin_page(tfdev, prn_stream, dev->width);
    tfax_set_fields(tfdev);
    TIFFSetField(tfdev->tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);

    return tiff_print_page(dev, tfdev->tif);
}

/* Print a PackBits page. */
static int
tiffpack_print_page(gx_device_printer * dev, FILE * prn_stream)
{
    gx_device_tfax *const tfdev = (gx_device_tfax *)dev;

    tfax_begin_page(tfdev, prn_stream, dev->width);
    tfax_set_fields(tfdev);
    TIFFSetField(tfdev->tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);

    return tiff_print_page(dev, tfdev->tif);
}

/* Begin a TIFF fax page. */
static int
tfax_begin_page(gx_device_tfax * tfdev, FILE * file, int width)
{
    gx_device_printer *const pdev = (gx_device_printer *)tfdev;
    /* Patch the width to reflect fax page width adjustment. */
    int save_width = tfdev->width;
    int code;

    /* open the TIFF device */
    if (gdev_prn_file_is_new(pdev)) {
	tfdev->tif = tiff_from_filep(pdev->dname, file, tfdev->BigEndian);
	if (!tfdev->tif)
	    return_error(gs_error_invalidfileaccess);
    }

    tfdev->width = width;
    code = tiff_set_fields_for_printer(pdev, tfdev->tif, tfdev->MaxStripSize);
    tfdev->width = save_width;
    return code;
}
