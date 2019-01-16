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


/* XPS interpreter - PNG image support */

#include "ghostxps.h"

#include "stream.h"
#include "strimpl.h"
#include "gsstate.h"

/* silence a warning where #if SHARE_LIBPNG is used when it's undefined */
#ifndef SHARE_LIBPNG
#define SHARE_LIBPNG 0
#endif
#include "png_.h"

/*
 * PNG using libpng directly (no gs wrappers)
 */

struct xps_png_io_s
{
    byte *ptr;
    byte *lim;
};

static void
xps_png_read(png_structp png, png_bytep data, png_size_t length)
{
    struct xps_png_io_s *io = png_get_io_ptr(png);
    if (io->ptr + length > io->lim)
        png_error(png, "Read Error");
    memcpy(data, io->ptr, length);
    io->ptr += length;
}

#define PNG_MEM_ALIGN 16
static png_voidp
xps_png_malloc(png_structp png, png_size_t size)
{
    gs_memory_t *mem = png_get_mem_ptr(png);
    uchar *unaligned;
    uchar *aligned;

    if (size == 0)
        return NULL;
    unaligned = gs_alloc_bytes(mem, size + PNG_MEM_ALIGN, "libpng");
    if (unaligned == NULL)
        return NULL;

    aligned = (uchar *)((intptr_t)(unaligned + PNG_MEM_ALIGN) & ~(PNG_MEM_ALIGN - 1));
    aligned[-1] = (uchar)(aligned - unaligned);

    return aligned;
}

static void
xps_png_free(png_structp png, png_voidp ptr)
{
    gs_memory_t *mem = png_get_mem_ptr(png);
    uchar *aligned = ptr;
    if (aligned == NULL)
        return;
    gs_free_object(mem, aligned - aligned[-1], "libpng");
}

/* This only determines if we have an alpha value */
int
xps_png_has_alpha(xps_context_t *ctx, byte *rbuf, int rlen)
{
    png_structp png;
    png_infop info;
    struct xps_png_io_s io;
    int has_alpha;

    /*
     * Set up PNG structs and input source
     */

    io.ptr = rbuf;
    io.lim = rbuf + rlen;

    png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
            NULL, NULL, NULL,
            ctx->memory, xps_png_malloc, xps_png_free);
    if (!png) {
        gs_warn("png_create_read_struct");
        return 0;
    }

    info = png_create_info_struct(png);
    if (!info) {
        gs_warn("png_create_info_struct");
        return 0;
    }

    png_set_read_fn(png, &io, xps_png_read);
    png_set_crc_action(png, PNG_CRC_WARN_USE, PNG_CRC_WARN_USE);

    /*
     * Jump to here on errors.
     */

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, NULL);
        gs_warn("png reading failed");
        return 0;
    }

    /*
     * Read PNG header
     */
    png_read_info(png, info);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
    {
        /* this will also expand the depth to 8-bits */
        png_set_tRNS_to_alpha(png);
    }
    png_read_update_info(png, info);

    switch (png_get_color_type(png, info))
    {
    case PNG_COLOR_TYPE_PALETTE:
    case PNG_COLOR_TYPE_GRAY:
    case PNG_COLOR_TYPE_RGB:
        has_alpha = 0;
        break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
    case PNG_COLOR_TYPE_RGB_ALPHA:
        has_alpha = 1;
        break;

    default:
        gs_warn("cannot handle this png color type");
        has_alpha = 0;
        break;
    }

    /*
     * Clean up memory.
     */

    png_destroy_read_struct(&png, &info, NULL);

    return has_alpha;
}

int
xps_decode_png(xps_context_t *ctx, byte *rbuf, int rlen, xps_image_t *image)
{
    png_structp png;
    png_infop info;
    struct xps_png_io_s io;
    int npasses;
    int pass;
    int y;
    int compression = 0; /* dummy as of libpng 1.5.x */
    char *iccname = NULL;
    char *iccprofile = NULL;
    unsigned int iccproflen = 0;
    png_uint_32 xres, yres;
    int unit;

    /*
     * Set up PNG structs and input source
     */

    io.ptr = rbuf;
    io.lim = rbuf + rlen;

    png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING,
            NULL, NULL, NULL,
            ctx->memory, xps_png_malloc, xps_png_free);
    if (!png)
        return gs_throw(-1, "png_create_read_struct");

    info = png_create_info_struct(png);
    if (!info)
        return gs_throw(-1, "png_create_info_struct");

    png_set_read_fn(png, &io, xps_png_read);
    png_set_crc_action(png, PNG_CRC_WARN_USE, PNG_CRC_WARN_USE);

    /*
     * Jump to here on errors.
     */

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, NULL);
        return gs_throw(-1, "png reading failed");
    }

    /*
     * Read PNG header
     */

    png_read_info(png, info);

    if (png_get_interlace_type(png, info) == PNG_INTERLACE_ADAM7)
    {
        npasses = png_set_interlace_handling(png);
    }
    else
    {
        npasses = 1;
    }

    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS))
    {
        /* this will also expand the depth to 8-bits */
        png_set_tRNS_to_alpha(png);
    }

    png_read_update_info(png, info);

    image->width = png_get_image_width(png, info);
    image->height = png_get_image_height(png, info);
    image->comps = png_get_channels(png, info);
    image->bits = png_get_bit_depth(png, info);

    /* See if we have an icc profile */
#if PNG_LIBPNG_VER_MINOR >= 5
    /* ignore the return value */
    (void)png_get_iCCP(png, info, (png_charpp)&iccname, &compression, (png_bytepp)&iccprofile, (png_uint_32 *)&iccproflen);
#else
    iccprofile = info->iccp_profile;
    iccproflen = info->iccp_proflen;
#endif
    if (iccprofile != NULL)
    {
        image->profilesize = iccproflen;
        image->profile = xps_alloc(ctx, iccproflen);
        if (image->profile)
        {
            /* If we can't create it, just ignore */
            memcpy(image->profile, iccprofile, iccproflen);
        }
    }

    switch (png_get_color_type(png, info))
    {
    case PNG_COLOR_TYPE_GRAY:
        image->colorspace = ctx->gray;
        image->hasalpha = 0;
        break;

    case PNG_COLOR_TYPE_RGB:
        image->colorspace = ctx->srgb;
        image->hasalpha = 0;
        break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
        image->colorspace = ctx->gray;
        image->hasalpha = 1;
        break;

    case PNG_COLOR_TYPE_RGB_ALPHA:
        image->colorspace = ctx->srgb;
        image->hasalpha = 1;
        break;

    default:
        return gs_throw(-1, "cannot handle this png color type");
    }

    /*
     * Extract DPI, default to 96 dpi
     */

    image->xres = 96;
    image->yres = 96;

    if (png_get_pHYs(png, info, &xres, &yres, &unit) & PNG_INFO_pHYs)
    {
        if (unit == PNG_RESOLUTION_METER)
        {
            image->xres = (int)(xres * 0.0254 + 0.5);
            image->yres = (int)(yres * 0.0254 + 0.5);
        }
    }

    /*
     * Read rows, filling transformed output into image buffer.
     */

    image->stride = (image->width * image->comps * image->bits + 7) / 8;

    image->samples = xps_alloc(ctx, image->stride * image->height);
    if (!image->samples) {
        return gs_throw(gs_error_VMerror, "out of memory.\n");
    }

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

    return gs_okay;
}
