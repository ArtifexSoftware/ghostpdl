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


/* PNG (Portable Network Graphics) Format.  Pronounced "ping". */

#include "gdevprn.h"
#include "gdevmem.h"
#include "gscdefs.h"
#include "gxgetbit.h"
#include "zlib.h"

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define X_DPI 72
#define Y_DPI 72

static dev_proc_print_page(fpng_print_page);

typedef struct gx_device_fpng_s gx_device_fpng;
struct gx_device_fpng_s {
    gx_device_common;
    gx_prn_device_common;
};

/* 24-bit color. */

/* Since the print_page doesn't alter the device, this device can print in the background */
static const gx_device_procs fpng_procs =
prn_color_params_procs(gdev_prn_open, gdev_prn_bg_output_page, gdev_prn_close,
                       gx_default_rgb_map_rgb_color,
                       gx_default_rgb_map_color_rgb,
                       gdev_prn_get_params, gdev_prn_put_params);
const gx_device_fpng gs_fpng_device =
{prn_device_body(gx_device_fpng, fpng_procs, "fpng",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 24, 255, 255, 256, 256, fpng_print_page),
};

/* ------ Private definitions ------ */

typedef struct fpng_buffer_s {
    int size;
    int compressed;
    unsigned char data[1];
} fpng_buffer_t;

static int fpng_init_buffer(void *arg, gx_device *dev, gs_memory_t *mem, int w, int h, void **pbuffer)
{
    /* Currently, we allocate a "worst case" buffer per band - this is
     * slightly larger than the band itself. We do this, so that in banded
     * mode, we never need to reallocate the buffer during operation.
     * For paged mode, we don't care so much about not reallocating - we
     * could spot paged mode by noting that w and h match that of the
     * device (or that of the device after downscaling at least), and then
     * allocate a smaller initial buffer. We could even output as we go
     * in paged mode. For now we leave this as an exercise for the reader.
     */
    fpng_buffer_t *buffer;
    int size = deflateBound(NULL, (w*3+1)*h);
    buffer = (fpng_buffer_t *)gs_alloc_bytes(mem, sizeof(fpng_buffer_t) + size, "fpng_init_buffer");
    *pbuffer = (void *)buffer;
    if (buffer == NULL)
        return gs_error_VMerror;
    buffer->size = size;
    buffer->compressed = 0;
    return 0;
}

static void fpng_free_buffer(void *arg, gx_device *dev, gs_memory_t *mem, void *buffer)
{
    gs_free_object(mem, buffer, "fpng_init_buffer");
}

static void big32(unsigned char *buf, unsigned int v)
{
    buf[0] = (v >> 24) & 0xff;
    buf[1] = (v >> 16) & 0xff;
    buf[2] = (v >> 8) & 0xff;
    buf[3] = (v) & 0xff;
}

static void write_big32(int v, FILE *file)
{
    fputc(v>>24, file);
    fputc(v>>16, file);
    fputc(v>>8, file);
    fputc(v>>0, file);
}

static void putchunk(char *tag, unsigned char *data, int size, FILE *file)
{
    unsigned int sum;
    write_big32(size, file);
    fwrite(tag, 1, 4, file);
    fwrite(data, 1, size, file);
    sum = crc32(0, NULL, 0);
    sum = crc32(sum, (unsigned char*)tag, 4);
    sum = crc32(sum, data, size);
    write_big32(sum, file);
}

static void *zalloc(void *mem_, unsigned int items, unsigned int size)
{
    gs_memory_t *mem = (gs_memory_t *)mem_;

    return gs_alloc_bytes(mem, items * size, "zalloc (fpng_process)");
}

static void zfree(void *mem_, void *address)
{
    gs_memory_t *mem = (gs_memory_t *)mem_;

    gs_free_object(mem, address, "zfree (fpng_process)");
}

static inline int paeth_predict(const unsigned char *d, int raster)
{
    int a = d[-3]; /* Left */
    int b = d[-raster]; /* Above */
    int c = d[-3-raster]; /* Above left */
    int p = a + b - c;
    int pa, pb, pc;
    pa = p - a;
    if (pa < 0)
        pa = -pa;
    pb = p - b;
    if (pb < 0)
        pb = -pb;
    pc = p - c;
    if (pc < 0)
        pc = -pc;
    if (pa <= pb && pa <= pc)
        return a;
    if (pb <= pc)
        return b;
    return c;
}

static int fpng_process(void *arg, gx_device *dev, gx_device *bdev, const gs_int_rect *rect, void *buffer_)
{
    int code;
    gs_get_bits_params_t params;
    int unread;
    int raster = bitmap_raster(dev->width * 3 * 8);
    int h = rect->q.y - rect->p.y;
    int w = rect->q.x - rect->p.x;
    int x, y;
    unsigned char *p;
    unsigned char sub = 1;
    unsigned char paeth = 4;
    int firstband = (rect->p.y == 0);
    int lastband = (rect->q.y == dev->height-1);
    gs_int_rect my_rect;
    z_stream stream;
    int err;
    fpng_buffer_t *buffer = (fpng_buffer_t *)buffer_;

    if (h <= 0 || w <= 0)
        return 0;

    params.options = GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_CHUNKY | GB_RETURN_POINTER | GB_ALIGN_ANY | GB_OFFSET_0 | GB_RASTER_STANDARD;
    my_rect.p.x = 0;
    my_rect.p.y = 0;
    my_rect.q.x = w;
    my_rect.q.y = h;
    code = dev_proc(bdev, get_bits_rectangle)(bdev, &my_rect, &params, NULL);
    if (code < 0)
        return code;

    /* Apply the paeth prediction filter to the buffered data */
    p = params.data[0];
    /* Paeth for lines h-1 to 1 */
    p += raster*(h-1);
    for (y = h-1; y > 0; y--)
    {
        p += 3*(w-1);
        for (x = w-1; x > 0; x--)
        {
            p[0] -= paeth_predict(p+0, raster);
            p[1] -= paeth_predict(p+1, raster);
            p[2] -= paeth_predict(p+2, raster);
            p -= 3;
        }
        p[0] -= p[-raster];
        p[1] -= p[1-raster];
        p[2] -= p[2-raster];
        p -= raster;
    }
    /* Sub for the first line */
    {
        p += 3*(w-1);
        for (x = w-1; x > 0; x--)
        {
            p[2] -= p[-1];
            p[1] -= p[-2];
            p[0] -= p[-3];
            p -= 3;
        }
    }

    /* Compress the data */
    stream.zalloc = zalloc;
    stream.zfree = zfree;
    stream.opaque = bdev->memory;
    err = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
    if (err != Z_OK)
        return gs_error_VMerror;
    p = params.data[0];
    stream.next_out = &buffer->data[0];
    stream.avail_out = buffer->size;

    /* Nasty zlib hackery here. Zlib always outputs a 'start of stream'
     * marker at the beginning. We just want a block, so for all blocks
     * except the first one, we compress a single byte and flush it. This
     * takes care of the 'start of stream' marker. Throw this deflate block
     * away, and start compression again. */
    if (!firstband)
    {
        stream.next_in = &sub;
        stream.avail_in = 1;
        deflate(&stream, Z_FULL_FLUSH);
        stream.next_out = &buffer->data[0];
        stream.avail_out = buffer->size;
        stream.total_out = 0;
    }

    stream.next_in = &sub;
    for (y = h-1; y >= 0; y--)
    {
        stream.avail_in = 1;
        deflate(&stream, Z_NO_FLUSH);
        stream.next_in = p;
        stream.avail_in = w*3;
        deflate(&stream, (y == 0 ? (lastband ? Z_FINISH : Z_FULL_FLUSH) : Z_NO_FLUSH));
        p += raster;
        stream.next_in = &paeth;
    }
    /* Ignore errors given here */
    deflateEnd(&stream);

    buffer->compressed = stream.total_out;

    return code;
}

static int fpng_output(void *arg, gx_device *dev, void *buffer_)
{
    FILE *file = (FILE *)arg;
    fpng_buffer_t *buffer = (fpng_buffer_t *)buffer_;

    putchunk("IDAT", &buffer->data[0], buffer->compressed, file);

    return 0;
}

/* Write out a page in PNG format. */
static int
fpng_print_page(gx_device_printer * pdev, FILE * file)
{
    gs_memory_t *mem = pdev->memory;
    static const unsigned char pngsig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    unsigned char head[13];
    gx_process_page_options_t process = { 0 };

    fwrite(pngsig, 1, 8, file); /* Signature */

    /* IHDR chunk */
    big32(&head[0], pdev->width);
    big32(&head[4], pdev->height);
    head[8] = 8; /* 8bpc */
    head[9] = 2; /* rgb */
    head[10] = 0; /* compression */
    head[11] = 0; /* filter */
    head[12] = 0; /* interlace */
    putchunk("IHDR", head, 13, file);

    process.init_buffer_fn = fpng_init_buffer;
    process.free_buffer_fn = fpng_free_buffer;
    process.process_fn = fpng_process;
    process.output_fn = fpng_output;
    process.arg = file;

    return dev_proc(pdev, process_page)(pdev, &process);
}
