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

/* $Id: gslt_image_png.c 2991 2007-12-18 23:05:58Z giles $ */
/* gslt image loading implementation for PNG images */

#include "std.h"
#include "gsmemory.h"
#include "stream.h"
#include "strimpl.h"
#include "gsstate.h"
#include "png_.h"
#include "gslt_image.h"

/* import the gs structure descriptor for gslt_image_t */
extern const gs_memory_struct_type_t st_gslt_image;

/*
 * PNG using libpng directly (no gs wrappers)
 */

struct gslt_png_io_s
{
    byte *ptr;
    byte *lim;
};

static void gslt_png_read(png_structp png, png_bytep data, png_size_t length)
{
    struct gslt_png_io_s *io = png_get_io_ptr(png);
    if (io->ptr + length > io->lim)
        png_error(png, "Read Error");
    memcpy(data, io->ptr, length);
    io->ptr += length;
}

static png_voidp gslt_png_malloc(png_structp png, png_size_t size)
{
    gs_memory_t *mem = png_get_mem_ptr(png);
    return gs_alloc_bytes(mem, size, "libpng");
}

static void gslt_png_free(png_structp png, png_voidp ptr)
{
    gs_memory_t *mem = png_get_mem_ptr(png);
    gs_free_object(mem, ptr, "libpng");
}

gslt_image_t *
gslt_image_decode_png(gs_memory_t *mem, byte *buf, int len)
{
    gslt_image_t *image;
    png_structp png;
    png_infop info;
    struct gslt_png_io_s io;
    int npasses;
    int pass;
    int y;

    /*
     * Set up PNG structs and input source
     */

    io.ptr = buf;
    io.lim = buf + len;

    png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
            NULL, NULL, NULL, /* error callbacks */
            mem, gslt_png_malloc, gslt_png_free);
    if (!png) {
        gs_throw(-1, "png_create_read_struct_2 failed");
        return NULL;
    }
    info = png_create_info_struct(png);
    if (!info) {
        gs_throw(-1, "png_create_info_struct");
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }
    png_set_read_fn(png, &io, gslt_png_read);

    /*
     * Jump to here on errors.
     */

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, NULL);
        gs_throw(-1, "png reading failed");
        return NULL;
    }

    /*
     * Read PNG header
     */

    png_read_info(png, info);

    image = gs_alloc_struct_immovable(mem, gslt_image_t,
        &st_gslt_image, "new png gslt_image");
    if (image == NULL) {
        gs_throw(-1, "unable to allocate png gslt_image");
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    image->width = png_get_image_width(png, info);
    image->height = png_get_image_height(png, info);
    image->bits = png_get_bit_depth(png, info);

    if (png_get_interlace_type(png, info) == PNG_INTERLACE_ADAM7)
    {
        npasses = png_set_interlace_handling(png);
    }
    else
    {
        npasses = 1;
    }

    if (image->bits == 16)
    {
        png_set_strip_16(png);
        image->bits = 8;
    }

    switch (png_get_color_type(png, info))
    {
    case PNG_COLOR_TYPE_GRAY:
        image->components = 1;
        image->colorspace = GSLT_GRAY;
        break;

    case PNG_COLOR_TYPE_PALETTE:
        /* ask libpng to expand palettes to rgb triplets */
        png_set_palette_to_rgb(png);
        image->bits = 8;

        /* libpng will expand to rgba if there is a tRNS chunk */
        if (png_get_valid(png, info, PNG_INFO_tRNS))
        {
            image->components = 4;
            image->colorspace = GSLT_RGB_A;
        }
        else
        {
            image->components = 3;
            image->colorspace = GSLT_RGB;
        }
        break;

    case PNG_COLOR_TYPE_RGB:
        image->components = 3;
        image->colorspace = GSLT_RGB;
        break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
        image->components = 2;
        image->colorspace = GSLT_GRAY_A;
        break;

    case PNG_COLOR_TYPE_RGB_ALPHA:
        image->components = 4;
        image->colorspace = GSLT_RGB_A;
        break;

    default:
        gs_throw(-1, "cannot handle this png color type");
        png_destroy_read_struct(&png, &info, NULL);
        gs_free_object(mem, image, "free png gslt_image");
        return NULL;
    }

    image->stride = (image->width * image->components * image->bits + 7) / 8;

    /*
     * Extract DPI, default to 96 dpi
     */

    image->xres = 96;
    image->yres = 96;

    if (info->valid & PNG_INFO_pHYs)
    {
        png_uint_32 xres, yres;
        int unit;
        png_get_pHYs(png, info, &xres, &yres, &unit);
        if (unit == PNG_RESOLUTION_METER)
        {
            image->xres = xres * 0.0254 + 0.5;
            image->yres = yres * 0.0254 + 0.5;
        }
    }

    /*
     * Read rows, filling transformed output into image buffer.
     */

    image->samples = gs_alloc_bytes(mem, image->stride * image->height, "decodepng");

    for (pass = 0; pass < npasses; pass++)
    {
        for (y = 0; y < image->height; y++)
        {
            png_read_row(png, image->samples + (y * image->stride), NULL);
        }
    }

    /*
     * Clean up memory.
     */

    png_destroy_read_struct(&png, &info, NULL);

    return image;
}
