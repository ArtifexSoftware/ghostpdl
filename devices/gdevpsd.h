/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

#ifndef gdevpsd_INCLUDED
# define gdevpsd_INCLUDED

#include "std.h"
#include "gdevdevnprn.h"

typedef struct {
    gp_file *f;

    int width;
    int height;
    int base_num_channels;	/* almost always 3 (rgb) or 4 (CMYK) */
    int n_extra_channels;
    int num_channels;	/* base_num_channels + any spot colors that are imaged */
    /* Map output channel number to original separation number. */
    int chnl_to_orig_sep[GX_DEVICE_COLOR_MAX_COMPONENTS];
    /* Map output channel number to gx_color_index position. */
    int chnl_to_position[GX_DEVICE_COLOR_MAX_COMPONENTS];
    /* byte offset of image data */
    int image_data_off;
} psd_write_ctx;

int psd_setup(psd_write_ctx *xc, gx_devn_prn_device *dev, gp_file *file, int w, int h);
int psd_write_header(psd_write_ctx *xc, gx_devn_prn_device *pdev);

int psd_write(psd_write_ctx *xc, const byte *buf, int size);
int psd_write_8(psd_write_ctx *xc, byte v);
int psd_write_16(psd_write_ctx *xc, bits16 v);
int psd_write_32(psd_write_ctx *xc, bits32 v);

bool
psd_allow_multiple_pages (gx_device_printer *pdev);

#endif		/* ifndef gdevdevnprn_INCLUDED */
