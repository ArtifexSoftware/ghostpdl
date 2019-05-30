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


/* PNG (Portable Network Graphics) Format.  Pronounced "ping". */

#include "zlib.h"
#include "gdevprn.h"
#include "gdevmem.h"
#include "gscdefs.h"
#include "gxgetbit.h"
#include "gxdownscale.h"
#include "gxdevsop.h"

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
    gx_downscaler_params downscale;
};

static int
fpng_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_fpng *pdev = (gx_device_fpng *)dev;
    gs_param_list * plist = (gs_param_list *)list;

    if (strcmp(Param, "DownScaleFactor") == 0) {
        return param_write_int(plist, "DownScaleFactor", &pdev->downscale.downscale_factor);
    }
    return gdev_prn_get_param(dev, Param, list);
}

static int
fpng_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_fpng *pdev = (gx_device_fpng *)dev;
    int code, ecode;

    ecode = 0;
    if ((code = gx_downscaler_write_params(plist, &pdev->downscale, 0)) < 0)
        ecode = code;

    code = gdev_prn_get_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
fpng_put_params(gx_device *dev, gs_param_list *plist)
{
    gx_device_fpng *pdev = (gx_device_fpng *)dev;
    int code, ecode;

    ecode = gx_downscaler_read_params(plist, &pdev->downscale, 0);

    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        ecode = code;

    return ecode;
}

static int
fpng_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    gx_device_fpng *fdev = (gx_device_fpng *)pdev;

    if (dev_spec_op == gxdso_adjust_bandheight)
        return gx_downscaler_adjust_bandheight(fdev->downscale.downscale_factor, size);

    if (dev_spec_op == gxdso_get_dev_param) {
        int code;
        dev_param_req_t *request = (dev_param_req_t *)data;
        code = fpng_get_param(pdev, request->Param, request->list);
        if (code != gs_error_undefined)
            return code;
    }

    return gdev_prn_dev_spec_op(pdev, dev_spec_op, data, size);
}

/* 24-bit color. */

/* Since the print_page doesn't alter the device, this device can print in the background */
static const gx_device_procs fpng_procs =
{
        gdev_prn_open,
        NULL,	/* get_initial_matrix */
        NULL,	/* sync_output */
        gdev_prn_bg_output_page,
        gdev_prn_close,
        gx_default_rgb_map_rgb_color,
        gx_default_rgb_map_color_rgb,
        NULL,	/* fill_rectangle */
        NULL,	/* tile_rectangle */
        NULL,	/* copy_mono */
        NULL,	/* copy_color */
        NULL,	/* draw_line */
        NULL,	/* get_bits */
        fpng_get_params,
        fpng_put_params,
        NULL,	/* map_cmyk_color */
        NULL,	/* get_xfont_procs */
        NULL,	/* get_xfont_device */
        NULL,	/* map_rgb_alpha_color */
        gx_page_device_get_page_device,
        NULL,	/* get_alpha_bits */
        NULL,	/* copy_alpha */
        NULL,	/* get_band */
        NULL,	/* copy_rop */
        NULL,	/* fill_path */
        NULL,	/* stroke_path */
        NULL,	/* fill_mask */
        NULL,	/* fill_trapezoid */
        NULL,	/* fill_parallelogram */
        NULL,	/* fill_triangle */
        NULL,	/* draw_thin_line */
        NULL,	/* begin_image */
        NULL,	/* image_data */
        NULL,	/* end_image */
        NULL,	/* strip_tile_rectangle */
        NULL,	/* strip_copy_rop, */
        NULL,	/* get_clipping_box */
        NULL,	/* begin_typed_image */
        NULL,	/* get_bits_rectangle */
        NULL,	/* map_color_rgb_alpha */
        NULL,	/* create_compositor */
        NULL,	/* get_hardware_params */
        NULL,	/* text_begin */
        NULL,	/* finish_copydevice */
        NULL,	/* begin_transparency_group */
        NULL,	/* end_transparency_group */
        NULL,	/* begin_transparency_mask */
        NULL,	/* end_transparency_mask */
        NULL,  /* discard_transparency_layer */
        NULL,  /* get_color_mapping_procs */
        NULL,  /* get_color_comp_index */
        NULL,  /* encode_color */
        NULL,  /* decode_color */
        NULL,  /* pattern_manage */
        NULL,  /* fill_rectangle_hl_color */
        NULL,  /* include_color_space */
        NULL,  /* fill_linear_color_scanline */
        NULL,  /* fill_linear_color_trapezoid */
        NULL,  /* fill_linear_color_triangle */
        NULL,  /* update_spot_equivalent_colors */
        NULL,  /* ret_devn_params */
        NULL,  /* fillpage */
        NULL,  /* push_transparency_state */
        NULL,  /* pop_transparency_state */
        NULL,  /* put_image */
        fpng_dev_spec_op,  /* dev_spec_op */
        NULL,  /* copy plane */
        gx_default_get_profile, /* get_profile */
        gx_default_set_graphics_type_tag /* set_graphics_type_tag */
};
const gx_device_fpng gs_fpng_device =
{prn_device_body(gx_device_fpng, fpng_procs, "fpng",
                 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                 X_DPI, Y_DPI,
                 0, 0, 0, 0,	/* margins */
                 3, 24, 255, 255, 256, 256, fpng_print_page),
                 GX_DOWNSCALER_PARAMS_DEFAULTS
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
      return_error(gs_error_VMerror);
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

static void write_big32(int v, gp_file *file)
{
    gp_fputc(v>>24, file);
    gp_fputc(v>>16, file);
    gp_fputc(v>>8, file);
    gp_fputc(v>>0, file);
}

static void putchunk(const char *tag, const unsigned char *data, int size, gp_file *file)
{
    unsigned int sum;
    write_big32(size, file);
    gp_fwrite(tag, 1, 4, file);
    gp_fwrite(data, 1, size, file);
    sum = crc32(0, NULL, 0);
    sum = crc32(sum, (const unsigned char*)tag, 4);
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
    gx_device_fpng *fdev = (gx_device_fpng *)dev;
    gs_get_bits_params_t params;
    int w = rect->q.x - rect->p.x;
    int raster = bitmap_raster(bdev->width * 3 * 8);
    int h = rect->q.y - rect->p.y;
    int x, y;
    unsigned char *p;
    unsigned char sub = 1;
    unsigned char paeth = 4;
    int firstband = (rect->p.y == 0);
    int lastband;
    gs_int_rect my_rect;
    z_stream stream;
    int err;
    int page_height = gx_downscaler_scale_rounded(dev->height, fdev->downscale.downscale_factor);
    fpng_buffer_t *buffer = (fpng_buffer_t *)buffer_;

    if (h <= 0 || w <= 0)
        return 0;

    lastband = (rect->q.y == page_height-1);

    params.options = GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_CHUNKY | GB_RETURN_POINTER | GB_ALIGN_ANY | GB_OFFSET_0 | GB_RASTER_ANY;
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
      return_error(gs_error_VMerror);
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
    gp_file *file = (gp_file *)arg;
    fpng_buffer_t *buffer = (fpng_buffer_t *)buffer_;

    putchunk("IDAT", &buffer->data[0], buffer->compressed, file);

    return 0;
}

/* Write out a page in PNG format. */
static int
fpng_print_page(gx_device_printer *pdev, gp_file *file)
{
    gx_device_fpng *fdev = (gx_device_fpng *)pdev;
    static const unsigned char pngsig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    unsigned char head[13];
    gx_process_page_options_t process = { 0 };

    gp_fwrite(pngsig, 1, 8, file); /* Signature */

    /* IHDR chunk */
    big32(&head[0], gx_downscaler_scale_rounded(pdev->width, fdev->downscale.downscale_factor));
    big32(&head[4], gx_downscaler_scale_rounded(pdev->height, fdev->downscale.downscale_factor));
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

    return gx_downscaler_process_page((gx_device *)pdev, &process, fdev->downscale.downscale_factor);
}
