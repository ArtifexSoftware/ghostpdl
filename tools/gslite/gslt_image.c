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

/* $Id: gslt_image.c 2991 2007-12-18 23:05:58Z giles $ */
/* gslt image loading implementation */

#include "memory_.h"
#include "gsmemory.h"
#include "gp.h"
#include "gslt_image.h"
#include "ctype_.h"
#include "strimpl.h"
#include "scommon.h"
#include "jpeglib_.h"           /* for jpeg filter */
#include "sdct.h"
#include "sjpeg.h"
#include "gdebug.h"
#include "gsutil.h"

/* gs memory management structure descriptor for gslt_image_t */
/* normally this would be in the header with the structure
   declaration, with which is must be synchronized, but we
   wish to hide such details from the client */
#define public_st_gslt_image() \
  gs_public_st_ptrs1(st_gslt_image, gslt_image_t,\
        "gslt_image_t", gslt_image_enum_ptrs, gslt_image_reloc_ptrs,\
        samples)

/* define the gslt_image_t structure descriptor */
public_st_gslt_image();

/*
 * Strip alpha channel from an image
 * assumes a collapsed stride
 */
static void
gslt_strip_alpha(gslt_image_t *image)
{
    gslt_image_colorspace cs = image->colorspace;
    int n = image->components;
    int y, x, k;
    byte *sp, *dp;

    if (image->bits != 8)
    {
        gs_warn1("cannot strip alpha from %dbpc images", image->bits);
        return;
    }

    if ((cs != GSLT_GRAY_A) && (cs != GSLT_RGB_A) && (cs != GSLT_CMYK_A))
        return;

    for (y = 0; y < image->height; y++)
    {
        sp = image->samples + image->width * n * y;
        dp = image->samples + image->width * (n - 1) * y;
        for (x = 0; x < image->width; x++)
        {
            for (k = 0; k < n - 1; k++)
                *dp++ = *sp++;
            sp++;
        }
    }

    image->colorspace --; /* assume foo_A follows foo */
    image->components --;
    image->stride = (n - 1) * image->width;
}

/*
 * Switch on file magic to decode an image.
 */
gslt_image_t *
gslt_image_decode(gs_memory_t *mem, byte *buf, int len)
{
    gslt_image_t *image = NULL;
    int error = gs_okay;

    if (buf[0] == 0xff && buf[1] == 0xd8)
        image = gslt_image_decode_jpeg(mem, buf, len);
    else if (memcmp(buf, "\211PNG\r\n\032\n", 8) == 0)
        image = gslt_image_decode_png(mem, buf, len);
    else if (memcmp(buf, "MM", 2) == 0)
        image = gslt_image_decode_tiff(mem, buf, len);
    else if (memcmp(buf, "II", 2) == 0)
        image = gslt_image_decode_tiff(mem, buf, len);
    else
        error = gs_throw(-1, "unknown image file format");

    if (image == NULL)
        error = gs_rethrow(error, "could not decode image");

    return image;
}

void
gslt_image_free(gs_memory_t *mem, gslt_image_t *image)
{
    if (image != NULL) {
        if (image->samples) {
            gs_free_object(mem, image->samples, "free gslt_image samples");
        }
        gs_free_object(mem, image, "free gslt_image");
    }
}
