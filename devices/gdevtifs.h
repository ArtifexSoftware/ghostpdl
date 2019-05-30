/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* Definitions for writing TIFF file formats. */

#ifndef gdevtifs_INCLUDED
#  define gdevtifs_INCLUDED

#include <tiffio.h>     /* must be first, because gdevio.h re-#defines "printf"
                           which is used in a function __attribute__ by
                           tiffio.h */
#include "gdevprn.h"
#include "gxdownscale.h"

/* ================ Implementation ================ */

typedef struct gx_device_tiff_s {
    gx_device_common;
    gx_prn_device_common;
    bool  BigEndian;            /* true = big endian; false = little endian*/
    bool  UseBigTIFF;           /* true = output big tiff file, false don't */
    uint16 Compression;         /* same values as TIFFTAG_COMPRESSION */
    long MaxStripSize;
    long AdjustWidth;            /* 0 = no adjust, 1 = adjust to fax values, >1 = adjust to this */
    bool write_datetime;
    gx_downscaler_params downscale;
    gsicc_link_t *icclink;
    TIFF *tif;                  /* TIFF file opened on gx_device_common.file */
} gx_device_tiff;

dev_proc_open_device(tiff_open);
dev_proc_close_device(tiff_close);
dev_proc_get_params(tiff_get_params);
dev_proc_get_params(tiff_get_params_downscale);
dev_proc_get_params(tiff_get_params_downscale_cmyk);
dev_proc_get_params(tiff_get_params_downscale_cmyk_ets);
dev_proc_put_params(tiff_put_params);
dev_proc_put_params(tiff_put_params_downscale);
dev_proc_put_params(tiff_put_params_downscale_cmyk);
dev_proc_put_params(tiff_put_params_downscale_cmyk_ets);

int tiff_print_page(gx_device_printer *dev, TIFF *tif, int min_feature_size);

int tiff_downscale_and_print_page(gx_device_printer *dev, TIFF *tif,
                                  int factor, int msf, int aw, int bpc,
                                  int num_comps,
                                  int trap_w, int trap_h, const int *trap_order,
                                  int ets);
void tiff_set_handlers (void);

/*
 * Sets the compression tag for TIFF and updates the rows_per_strip tag to
 * reflect max_strip_size under the new compression scheme.
 */
#define TIFF_DEFAULT_STRIP_SIZE 1048576

#define TIFF_DEFAULT_DOWNSCALE 1

int tiff_set_compression(gx_device_printer *pdev,
                         TIFF *tif,
                         uint compression,
                         long max_strip_size);

int tiff_set_fields_for_printer(gx_device_printer *pdev, TIFF *tif, int factor,
                                int adjustWidth, bool writedatetime);

int gdev_tiff_begin_page(gx_device_tiff *tfdev, gp_file *file);

/*
 * Returns the gs_param_string that corresponds to the tiff COMPRESSION_* id.
 */
int tiff_compression_param_string(gs_param_string *param, uint16 id);

/*
 * Returns the COMPRESSION_* id which corresponds to 'str'.
 */
int tiff_compression_id(uint16 *id, gs_param_string *param);

/*
 * Returns true if 'compression' can be used for encoding a data with a bit
 * depth of 'depth'  (crle, g3, and g4 only work on 1-bit devices).
 */
int tiff_compression_allowed(uint16 compression, byte depth);

#endif /* gdevtifs_INCLUDED */
