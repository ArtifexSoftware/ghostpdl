/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

#include "stdint_.h"
#include "gdevprn.h"
#include "gxdownscale.h"
#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and obejct filter */
#include "stream.h"
#include "spprint.h"
#include "time_.h"
#include "smd5.h"
#include "sstring.h"
#include "strimpl.h"
#include "slzwx.h"
#include "szlibx.h"
#include "sdct.h"
#include "srlx.h"
#include "gsicc_cache.h"
#include "sjpeg.h"

#include "gdevpdfimg.h"

enum {
    COMPRESSION_NONE	= 1,	/* dump mode */
    COMPRESSION_LZW	= 2,    /* Lempel-Ziv & Welch */
    COMPRESSION_FLATE	= 3,
    COMPRESSION_JPEG	= 4,
    COMPRESSION_RLE	= 5
};

static struct compression_string {
    unsigned char id;
    const char *str;
} compression_strings [] = {
    { COMPRESSION_NONE, "None" },
    { COMPRESSION_LZW, "LZW" },     /* Not supported in PCLm */
    { COMPRESSION_FLATE, "Flate" },
    { COMPRESSION_JPEG, "JPEG" },
    { COMPRESSION_RLE, "RLE" },
    { 0, NULL }
};

int PCLm_open(gx_device *pdev);
int PCLm_close(gx_device * pdev);


/* ------ The pdfimage8 device ------ */
static void
pdfimage8_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, open_device, pdf_image_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page);
    set_dev_proc(dev, close_device, pdf_image_close);
    set_dev_proc(dev, get_params, pdf_image_get_params_downscale);
    set_dev_proc(dev, put_params, pdf_image_put_params_downscale);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_pdf_image gs_pdfimage8_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfimage8_initialize_device_procs,
                    "pdfimage8",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    1,          /* num components */
                    8,          /* bits per sample */
                    255, 0, 256, 0,
                    pdf_image_print_page),
                    3,
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0,  /* StripHeight */
    0.0, /* QFactor */
    0    /* JPEGQ */
};

/* ------ The pdfimage24 device ------ */
static void
pdfimage24_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, open_device, pdf_image_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page);
    set_dev_proc(dev, close_device, pdf_image_close);
    set_dev_proc(dev, get_params, pdf_image_get_params_downscale);
    set_dev_proc(dev, put_params, pdf_image_put_params_downscale);
}

const gx_device_pdf_image gs_pdfimage24_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfimage24_initialize_device_procs,
                    "pdfimage24",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    3,          /* num components */
                    24,         /* bits per sample */
                    255, 255, 256, 256,
                    pdf_image_print_page),
                    3,
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0,  /* StripHeight */
    0.0, /* QFactor */
    0    /* JPEGQ */
};

/* ------ The pdfimage32 device ------ */
static void
pdfimage32_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, open_device, pdf_image_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page);
    set_dev_proc(dev, close_device, pdf_image_close);
    set_dev_proc(dev, get_params, pdf_image_get_params_downscale_cmyk);
    set_dev_proc(dev, put_params, pdf_image_put_params_downscale_cmyk);
}

const gx_device_pdf_image gs_pdfimage32_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfimage32_initialize_device_procs,
                    "pdfimage32",
                    DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                    600, 600,   /* 600 dpi by default */
                    0, 0, 0, 0, /* Margins */
                    4,          /* num components */
                    32,         /* bits per sample */
                    255, 255, 256, 256,
                    pdf_image_print_page),
                    3,
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    0,  /* StripHeight */
    0.0, /* QFactor */
    0    /* JPEGQ */
};

static int
pdfimage_write_args_comment(gx_device_pdf_image *pdev, stream *s)
{
    const char * const *argv = NULL;
    const char *arg;
    int towrite, length, i, j, argc;

    argc = gs_lib_ctx_get_args(pdev->memory->gs_lib_ctx, &argv);

    stream_write(s, (byte *)"%%Invocation:", 13);
    length = 12;
    for (i=0;i < argc; i++) {
        arg = argv[i];

        if ((strlen(arg) + length) > 255) {
            stream_write(s, (byte *)"\n%%+ ", 5);
            length = 5;
        } else {
            stream_write(s, (byte *)" ", 1);
            length++;
        }

        if (strlen(arg) > 250)
            towrite = 250;
        else
            towrite = strlen(arg);

        length += towrite;

        for (j=0;j < towrite;j++) {
            if (arg[j] == 0x0A) {
                stream_write(s, (byte *)"<0A>", 4);
            } else {
                if (arg[j] == 0x0D) {
                    stream_write(s, (byte *)"<0D>", 4);
                } else {
                    stream_write(s, (byte *)&arg[j], 1);
                }
            }
        }
    }
    stream_write(s, (byte *)"\n", 1);
    return 0;
}

static int gdev_pdf_image_begin_page(gx_device_pdf_image *pdf_dev,
                         gp_file *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)pdf_dev;
    int code;
    pdfimage_page *page;

    page = (pdfimage_page *)gs_alloc_bytes(pdf_dev->memory->non_gc_memory, sizeof(pdfimage_page), "pdfimage create new page");
    if (page == NULL)
        return_error(gs_error_VMerror);

    memset(page, 0x00, sizeof(pdfimage_page));

    if (gdev_prn_file_is_new(pdev)) {
        /* Set up the icc link settings at this time */
        code = gx_downscaler_create_post_render_link((gx_device *)pdev,
                                                     &pdf_dev->icclink);
        if (code < 0) {
            gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
            return code;
        }

        /* Set up the stream and insert the file header */
        pdf_dev->strm = s_alloc(pdf_dev->memory->non_gc_memory, "pdfimage_open_temp_stream(strm)");
        if (pdf_dev->strm == 0) {
            gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
            return_error(gs_error_VMerror);
        }
        pdf_dev->strm_buf = gs_alloc_bytes(pdf_dev->memory->non_gc_memory, pdf_dev->width * (pdf_dev->color_info.depth / 8),
                                       "pdfimage_open_temp_stream(strm_buf)");
        if (pdf_dev->strm_buf == NULL) {
            pdf_dev->strm->file = NULL; /* Don't close underlying file when we free the stream */
            gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm,
                           "pdfimage_open_temp_stream(strm)");
            pdf_dev->strm = NULL;
            gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
            return_error(gs_error_VMerror);
        }
        swrite_file(pdf_dev->strm, pdf_dev->file, pdf_dev->strm_buf, pdf_dev->width * (pdf_dev->color_info.depth / 8));

        stream_puts(pdf_dev->strm, "%PDF-1.3\n");
        stream_puts(pdf_dev->strm, "%\307\354\217\242\n");
        pdfimage_write_args_comment(pdf_dev,pdf_dev->strm);

        if (pdf_dev->ocr.file_init) {
            code = pdf_dev->ocr.file_init(pdf_dev);
            if (code < 0) {
                gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm_buf, "pdfimage_open_temp_stream(strm_buf)");
                pdf_dev->strm->file = NULL; /* Don't close underlying file when we free the stream */
                gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm,
                               "pdfimage_open_temp_stream(strm)");
                pdf_dev->strm = NULL;
                gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
                return code;
            }
        }

        pdf_dev->Pages = page;
    } else {
        pdfimage_page *current = pdf_dev->Pages;
        while(current->next)
            current = current->next;
        current->next = page;
    }
    page->ImageObjectNumber = (pdf_dev->NumPages * PDFIMG_OBJS_PER_PAGE) + PDFIMG_STATIC_OBJS + pdf_dev->ocr.file_objects;
    page->LengthObjectNumber = page->ImageObjectNumber + 1;
    page->PageStreamObjectNumber = page->LengthObjectNumber + 1;
    page->PageDictObjectNumber = page->PageStreamObjectNumber + 1;
    page->PageLengthObjectNumber = page->PageDictObjectNumber + 1;
    page->ImageOffset = stell(pdf_dev->strm);
    pdf_dev->StripHeight = pdf_dev->height;

    return 0;
}
static int pdf_image_chunky_post_cm(void  *arg, byte **dst, byte **src, int w, int h,
    int raster)
{
    gsicc_bufferdesc_t input_buffer_desc, output_buffer_desc;
    gsicc_link_t *icclink = (gsicc_link_t*)arg;

    gsicc_init_buffer(&input_buffer_desc, icclink->num_input, 1, false,
        false, false, 0, raster, h, w);
    gsicc_init_buffer(&output_buffer_desc, icclink->num_output, 1, false,
        false, false, 0, raster, h, w);
    icclink->procs.map_buffer(NULL, icclink, &input_buffer_desc, &output_buffer_desc,
        src[0], dst[0]);
    return 0;
}

static int
encode(gx_device *pdev, stream **s, const stream_template *t, gs_memory_t *mem)
{
    gx_device_pdf_image *pdf_dev = (gx_device_pdf_image *)pdev;
    stream_state *st;

    if (t == &s_DCTE_template) {
        int code;
        stream_DCT_state *sDCT;
        jpeg_compress_data *jcdp;

        st = s_alloc_state(mem, t->stype, "pdfimage.encode");
        if (st == 0)
            return_error(gs_error_VMerror);

        sDCT = (stream_DCT_state *)st;
        st->templat = t;
        if (t->set_defaults)
            t->set_defaults(st);

        jcdp = gs_alloc_struct_immovable(mem, jpeg_compress_data,
           &st_jpeg_compress_data, "zDCTE");
        if (jcdp == 0) {
            gs_free_object(mem, st, "pdfimage.encode");
            return_error(gs_error_VMerror);
        }
        sDCT->data.compress = jcdp;
        sDCT->icc_profile = NULL;
        jcdp->memory = sDCT->jpeg_memory = mem;	/* set now for allocation */
        if ((code = gs_jpeg_create_compress(sDCT)) < 0) {
            gs_jpeg_destroy(sDCT);
            gs_free_object(mem, jcdp, "setup_image_compression");
            sDCT->data.compress = NULL; /* Avoid problems with double frees later */
            return code;
        }

        jcdp->Picky = 0;
        jcdp->Relax = 0;
        jcdp->cinfo.image_width = gx_downscaler_scale(pdf_dev->width, pdf_dev->downscale.downscale_factor);
        jcdp->cinfo.image_height = pdf_dev->StripHeight;
        switch (pdf_dev->color_info.depth) {
            case 32:
                jcdp->cinfo.input_components = 4;
                jcdp->cinfo.in_color_space = JCS_CMYK;
                break;
            case 24:
                jcdp->cinfo.input_components = 3;
                jcdp->cinfo.in_color_space = JCS_RGB;
                break;
            case 8:
                jcdp->cinfo.input_components = 1;
                jcdp->cinfo.in_color_space = JCS_GRAYSCALE;
                break;
        }
        if ((code = gs_jpeg_set_defaults(sDCT)) < 0) {
            gs_jpeg_destroy(sDCT);
            gs_free_object(mem, jcdp, "setup_image_compression");
            sDCT->data.compress = NULL; /* Avoid problems with double frees later */
            return code;
        }
        if (pdf_dev->JPEGQ > 0) {
            code = gs_jpeg_set_quality(sDCT, pdf_dev->JPEGQ, TRUE);
            if (code < 0) {
                gs_jpeg_destroy(sDCT);
                gs_free_object(mem, jcdp, "setup_image_compression");
                sDCT->data.compress = NULL; /* Avoid problems with double frees later */
                return code;
            }
        } else if (pdf_dev->QFactor > 0.0) {
            code = gs_jpeg_set_linear_quality(sDCT,
                                         (int)(min(pdf_dev->QFactor, 100.0)
                                               * 100.0 + 0.5),
                                              TRUE);
            if (code < 0) {
                gs_jpeg_destroy(sDCT);
                gs_free_object(mem, jcdp, "setup_image_compression");
                sDCT->data.compress = NULL; /* Avoid problems with double frees later */
                return code;
            }
        }
        jcdp->cinfo.write_JFIF_header = FALSE;
        jcdp->cinfo.write_Adobe_marker = FALSE;	/* must do it myself */

        jcdp->templat = s_DCTE_template;
        /* Make sure we get at least a full scan line of input. */
        sDCT->scan_line_size = jcdp->cinfo.input_components *
            jcdp->cinfo.image_width;
        jcdp->templat.min_in_size =
            max(s_DCTE_template.min_in_size, sDCT->scan_line_size);
        /* Make sure we can write the user markers in a single go. */
        jcdp->templat.min_out_size =
            max(s_DCTE_template.min_out_size, sDCT->Markers.size);
        if (s_add_filter(s, &jcdp->templat, st, mem) == 0) {
            gs_jpeg_destroy(sDCT);
            gs_free_object(mem, jcdp, "setup_image_compression");
            sDCT->data.compress = NULL; /* Avoid problems with double frees later */
            return_error(gs_error_VMerror);
        }
    } else {
        st = s_alloc_state(mem, t->stype, "pdfimage.encode");
        if (st == 0)
            return_error(gs_error_VMerror);
        if (t->set_defaults)
            t->set_defaults(st);
        if (s_add_filter(s, t, st, mem) == 0) {
            gs_free_object(mem, st, "pdfimage.encode");
            return_error(gs_error_VMerror);
        }
    }

    return 0;
}

static int
pdf_image_downscale_and_print_page(gx_device_printer *dev,
                                   gx_downscaler_params *params,
                                   int bpc, int num_comps)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int code = 0;
    byte *data = NULL;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = size;
    int row;
    int factor = params->downscale_factor;
    int height = gx_downscaler_scale(dev->height, factor);
    int width = gx_downscaler_scale(dev->width, factor);
    gx_downscaler_t ds;
    gs_offset_t stream_pos = 0;
    pdfimage_page *page = pdf_dev->Pages;
    char Buffer[1024];
    stream *s = pdf_dev->strm;
    gs_offset_t len;

    if (page == NULL)
        return_error(gs_error_undefined);

    while(page->next)
        page = page->next;

    if (num_comps != 4)
        params->trap_w = params->trap_h = 0;
    if (pdf_dev->icclink == NULL) {
        code = gx_downscaler_init(&ds, (gx_device *)dev,
                                  8, bpc, num_comps,
                                  params,
                                  NULL, /*&fax_adjusted_width*/
                                  0 /*aw*/);
    } else {
        code = gx_downscaler_init_cm(&ds, (gx_device *)dev,
                                     8, bpc, num_comps,
                                     params,
                                     NULL, /*&fax_adjusted_width*/
                                     0, /*aw*/
                                     pdf_image_chunky_post_cm,
                                     pdf_dev->icclink,
                                     pdf_dev->icclink->num_output);
    }
    if (code < 0)
        return code;

    data = gs_alloc_bytes(dev->memory, max_size, "pdf_image_print_page(data)");
    if (data == NULL) {
        gx_downscaler_fin(&ds);
        return_error(gs_error_VMerror);
    }

    if (pdf_dev->ocr.begin_page) {
        code = pdf_dev->ocr.begin_page(pdf_dev, width, height, num_comps*8);
        if (code < 0)
        {
            gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
            gx_downscaler_fin(&ds);
            return code;
        }
    }

    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Length %d 0 R\n", page->LengthObjectNumber);
    stream_puts(pdf_dev->strm, "/Subtype /Image\n");
    pprintd1(pdf_dev->strm, "/Width %d\n", width);
    pprintd1(pdf_dev->strm, "/Height %d\n", height);
    switch(num_comps) {
        case 1:
            stream_puts(pdf_dev->strm, "/ColorSpace /DeviceGray\n");
            stream_puts(pdf_dev->strm, "/BitsPerComponent 8\n");
            break;
        case 3:
            stream_puts(pdf_dev->strm, "/ColorSpace /DeviceRGB\n");
            stream_puts(pdf_dev->strm, "/BitsPerComponent 8\n");
            break;
        case 4:
            stream_puts(pdf_dev->strm, "/ColorSpace /DeviceCMYK\n");
            stream_puts(pdf_dev->strm, "/BitsPerComponent 8\n");
            break;
    }
    switch (pdf_dev->Compression) {
        case COMPRESSION_LZW:
            stream_puts(pdf_dev->strm, "/Filter /LZWDecode\n");
            stream_puts(pdf_dev->strm, ">>\nstream\n");
            stream_pos = stell(pdf_dev->strm);
            encode((gx_device *)pdf_dev, &pdf_dev->strm, &s_LZWE_template, pdf_dev->memory->non_gc_memory);
            break;
        case COMPRESSION_FLATE:
            stream_puts(pdf_dev->strm, "/Filter /FlateDecode\n");
            stream_puts(pdf_dev->strm, ">>\nstream\n");
            stream_pos = stell(pdf_dev->strm);
            encode((gx_device *)pdf_dev, &pdf_dev->strm, &s_zlibE_template, pdf_dev->memory->non_gc_memory);
            break;
        case COMPRESSION_JPEG:
            stream_puts(pdf_dev->strm, "/Filter /DCTDecode\n");
            stream_puts(pdf_dev->strm, ">>\nstream\n");
            stream_pos = stell(pdf_dev->strm);
            encode((gx_device *)pdf_dev, &pdf_dev->strm, &s_DCTE_template, pdf_dev->memory->non_gc_memory);
            break;
        case COMPRESSION_RLE:
            stream_puts(pdf_dev->strm, "/Filter /RunLengthDecode\n");
            stream_puts(pdf_dev->strm, ">>\nstream\n");
            stream_pos = stell(pdf_dev->strm);
            encode((gx_device *)pdf_dev, &pdf_dev->strm, &s_RLE_template, pdf_dev->memory->non_gc_memory);
            break;
        default:
        case COMPRESSION_NONE:
            stream_puts(pdf_dev->strm, ">>\nstream\n");
            stream_pos = stell(pdf_dev->strm);
            break;
    }


    for (row = 0; row < height && code >= 0; row++) {
        code = gx_downscaler_getbits(&ds, data, row);
        if (code < 0)
            break;
        if (pdf_dev->ocr.line)
           pdf_dev->ocr.line(pdf_dev, data);
        stream_write(pdf_dev->strm, data, width * num_comps);
    }

    if (code < 0) {
        gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
        gx_downscaler_fin(&ds);
        return code;
    }

    switch(pdf_dev->Compression) {
        case COMPRESSION_LZW:
        case COMPRESSION_FLATE:
        case COMPRESSION_JPEG:
        case COMPRESSION_RLE:
            s_close_filters(&pdf_dev->strm, s);
            break;
        default:
        case COMPRESSION_NONE:
            break;
    }

    stream_puts(pdf_dev->strm, "\nendstream\nendobj\n");
    page->LengthOffset = stell(pdf_dev->strm);

    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->LengthObjectNumber);
    pprintd1(pdf_dev->strm, "%d\n", page->LengthOffset - stream_pos - 18); /* 18 is the length of \nendstream\nendobj\n we need to take that off for the stream length */
    stream_puts(pdf_dev->strm, "endobj\n");

    page->PageStreamOffset = stell(pdf_dev->strm);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageStreamObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Filter/FlateDecode/Length %d 0 R\n>>\nstream\n", page->PageLengthObjectNumber);
    stream_pos = stell(pdf_dev->strm);
    encode((gx_device *)pdf_dev, &pdf_dev->strm, &s_zlibE_template, pdf_dev->memory->non_gc_memory);
    if (pdf_dev->ocr.end_page)
        stream_puts(pdf_dev->strm, "q\n");
    pprintd2(pdf_dev->strm, "%d 0 0 %d 0 0 cm\n/Im1 Do",
             (int)((width / (pdf_dev->HWResolution[0] / 72)) * factor),
             (int)((height / (pdf_dev->HWResolution[1] / 72)) * factor));
    if (pdf_dev->ocr.end_page) {
        stream_puts(pdf_dev->strm, "\nQ");
        pdf_dev->ocr.end_page(pdf_dev);
    }
    s_close_filters(&pdf_dev->strm, s);
    len = stell(pdf_dev->strm) - stream_pos;
    stream_puts(pdf_dev->strm, "\nendstream\nendobj\n");

    page->PageLengthOffset = stell(pdf_dev->strm);
    pprintd2(pdf_dev->strm, "%d 0 obj\n%d\nendobj\n", page->PageLengthObjectNumber, len);

    page->PageDictOffset = stell(pdf_dev->strm);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageDictObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Contents %d 0 R\n", page->PageStreamObjectNumber);
    stream_puts(pdf_dev->strm, "/Type /Page\n/Parent 2 0 R\n");
    gs_snprintf(Buffer, sizeof(Buffer), "/MediaBox [0 0 %f %f]\n", ((double)pdf_dev->width / pdf_dev->HWResolution[0]) * 72, ((double)pdf_dev->height / pdf_dev->HWResolution[1]) * 72);
    stream_puts(pdf_dev->strm, Buffer);
    pprintd1(pdf_dev->strm, "/Resources <<\n/XObject <<\n/Im1 %d 0 R\n>>\n", page->ImageObjectNumber);
    if (pdf_dev->ocr.file_init)
        pprintd1(pdf_dev->strm, "/Font <<\n/Ft0 %d 0 R\n>>\n", PDFIMG_STATIC_OBJS);
    stream_puts(pdf_dev->strm, ">>\n>>\nendobj\n");

    gx_downscaler_fin(&ds);
    gs_free_object(dev->memory, data, "pdf_image_print_page(data)");

    pdf_dev->NumPages++;
    return code;
}

static void write_fileID(stream *s, const byte *str, int size)
{
    const stream_template *templat;
    stream_AXE_state state;
    stream_state *st = NULL;

    templat = &s_AXE_template;
    st = (stream_state *) & state;
    s_AXE_init_inline(&state);
    stream_putc(s, '<');
    {
        byte buf[100];		/* size is arbitrary */
        stream_cursor_read r;
        stream_cursor_write w;
        int status;

        stream_cursor_read_init(&r, str, size);

        do {
            stream_cursor_write_init(&w, buf, sizeof(buf));
            status = (*templat->process) (st, &r, &w, true);
            stream_write(s, buf, (uint) (w.ptr + 1 - buf));
        }
        while (status == 1);
    }
}

static int
pdf_compute_fileID(gx_device_pdf_image * pdev, byte fileID[16], char *CreationDate, char *Title, char *Producer)
{
    /* We compute a file identifier when beginning a document
       to allow its usage with PDF encryption. Due to that,
       in contradiction to the Adobe recommendation, our
       ID doesn't depend on the document size.
    */
    gs_memory_t *mem = pdev->memory->non_gc_memory;
    uint ignore;
    stream *s = s_MD5E_make_stream(mem, fileID, 16);
    long secs_ns[2];

    if (s == NULL)
        return_error(gs_error_VMerror);

#ifdef CLUSTER
    secs_ns[0] = 0;
    secs_ns[1] = 0;
#else
    gp_get_realtime(secs_ns);
#endif
    sputs(s, (byte *)secs_ns, sizeof(secs_ns), &ignore);
#ifdef CLUSTER
    /* Don't have the ID's vary by filename output in the cluster testing.
     * This prevents us comparing gs to gpdl results, and makes it harder
     * to manually reproduce results. */
    sputs(s, (const byte *)"ClusterTest.pdf", strlen("ClusterTest.pdf"), &ignore);
#else
    sputs(s, (const byte *)pdev->fname, strlen(pdev->fname), &ignore);
#endif

    stream_puts(s, "/ModDate ");
    stream_puts(s, CreationDate);
    stream_puts(s, "\n/CreationDate ");
    stream_puts(s, CreationDate);
    stream_puts(s, "\n/Title (");
    stream_puts(s, Title);
    stream_puts(s, ")\n/Producer (");
    stream_puts(s, Producer);
    stream_puts(s, ")\n");

    sclose(s);
    gs_free_object(mem, s, "pdf_compute_fileID");
    return 0;
}

static void write_xref_entry (stream *s, gs_offset_t Offset)
{
    char O[11];
    int i;

    if (Offset > 9999999999){
        Offset = 0;
    }
    gs_snprintf(O, sizeof(O), "%d", Offset);
    for (i=0; i< (10 - strlen(O)); i++)
        stream_puts(s, "0");
    stream_puts(s, O);
    stream_puts(s, " 00000 n \n");
}

static void
pdf_store_default_Producer(char buf[256])
{
    int major = (int)(gs_revision / 1000);
    int minor = (int)(gs_revision - (major * 1000)) / 10;
    int patch = gs_revision % 10;

    gs_snprintf(buf, 256, "(%s %d.%02d.%d)", gs_product, major, minor, patch);
}

static int pdf_image_finish_file(gx_device_pdf_image *pdf_dev, int PCLm)
{
    pdfimage_page *page = pdf_dev->Pages;
    struct tm tms;
    time_t t;
    int timeoffset;
    char timesign;
    char CreationDate[26], Title[] = "Untitled", Producer[256];

    if (pdf_dev->strm != NULL) {
        byte fileID[16];

        pdf_store_default_Producer(Producer);

        pdf_dev->RootOffset = stell(pdf_dev->strm);
        stream_puts(pdf_dev->strm, "1 0 obj\n<<\n/Pages 2 0 R\n/Type /Catalog\n/Info 3 0 R\n>>\nendobj\n");

        pdf_dev->PagesOffset = stell(pdf_dev->strm);
        pprintd1(pdf_dev->strm, "2 0 obj\n<<\n/Count %d\n", pdf_dev->NumPages);
        stream_puts(pdf_dev->strm, "/Kids [");

        while(page){
            pprintd1(pdf_dev->strm, "%d 0 R ", page->PageDictObjectNumber);
            page = page->next;
        }

        stream_puts(pdf_dev->strm, "]\n/Type /Pages\n>>\nendobj\n");

#ifdef CLUSTER
        memset(&t, 0, sizeof(t));
        memset(&tms, 0, sizeof(tms));
        timesign = 'Z';
        timeoffset = 0;
#else
        time(&t);
        tms = *gmtime(&t);
        tms.tm_isdst = -1;
        timeoffset = (int)difftime(t, mktime(&tms)); /* tz+dst in seconds */
        timesign = (timeoffset == 0 ? 'Z' : timeoffset < 0 ? '-' : '+');
        timeoffset = any_abs(timeoffset) / 60;
        tms = *localtime(&t);
#endif

        gs_snprintf(CreationDate, sizeof(CreationDate), "(D:%04d%02d%02d%02d%02d%02d%c%02d\'%02d\')",
            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
            tms.tm_hour, tms.tm_min, tms.tm_sec,
            timesign, timeoffset / 60, timeoffset % 60);

        pdf_dev->InfoOffset = stell(pdf_dev->strm);
        stream_puts(pdf_dev->strm, "3 0 obj\n<<\n/Producer");
        stream_puts(pdf_dev->strm, Producer);
        stream_puts(pdf_dev->strm, "\n/CreationDate");
        stream_puts(pdf_dev->strm, CreationDate);
        stream_puts(pdf_dev->strm, "\n>>\nendobj\n");

        pdf_dev->xrefOffset = stell(pdf_dev->strm);
        if (PCLm)
            pprintd1(pdf_dev->strm, "xref\n0 %d\n0000000000 65536 f \n", pdf_dev->NextObject);
        else
            pprintd1(pdf_dev->strm, "xref\n0 %d\n0000000000 65536 f \n", (pdf_dev->NumPages * PDFIMG_OBJS_PER_PAGE) + PDFIMG_STATIC_OBJS + pdf_dev->ocr.file_objects);
        write_xref_entry(pdf_dev->strm, pdf_dev->RootOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->PagesOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->InfoOffset);
        if (pdf_dev->ocr.file_objects) {
            int i;

            for (i = 0; i < OCR_MAX_FILE_OBJECTS; i++)
                if (pdf_dev->ocr.file_object_offset[i])
                    write_xref_entry(pdf_dev->strm, pdf_dev->ocr.file_object_offset[i]);
        }

        if (!PCLm) {
            page = pdf_dev->Pages;
            while(page){
                write_xref_entry(pdf_dev->strm, page->ImageOffset);
                write_xref_entry(pdf_dev->strm, page->LengthOffset);
                write_xref_entry(pdf_dev->strm, page->PageStreamOffset);
                write_xref_entry(pdf_dev->strm, page->PageDictOffset);
                write_xref_entry(pdf_dev->strm, page->PageLengthOffset);
                page = page->next;
            }
            pprintd1(pdf_dev->strm, "trailer\n<<\n/Size %d\n/Root 1 0 R\n/ID [", (pdf_dev->NumPages * PDFIMG_OBJS_PER_PAGE) + PDFIMG_STATIC_OBJS + pdf_dev->ocr.file_objects);
            pdf_compute_fileID(pdf_dev, fileID, CreationDate, Title, Producer);
            write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
            write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
            pprintd1(pdf_dev->strm, "]\n>>\nstartxref\n%d\n%%%%EOF\n", pdf_dev->xrefOffset);
        } else {
            gs_offset_t streamsize, R = 0;
            char Buffer[1024];

            sflush(pdf_dev->xref_stream.strm);
            streamsize = gp_ftell(pdf_dev->xref_stream.file);
            if (gp_fseek(pdf_dev->xref_stream.file, 0, SEEK_SET) != 0)
                return_error(gs_error_ioerror);

            while(streamsize > 0) {
                if (streamsize > 1024) {
                    streamsize -= gp_fpread(Buffer, 1024, R, pdf_dev->xref_stream.file);
                    R += 1024;
                    stream_write(pdf_dev->strm, Buffer, 1024);
                }
                else {
                    gp_fpread(Buffer, streamsize, R, pdf_dev->xref_stream.file);
                    stream_write(pdf_dev->strm, Buffer, streamsize);
                    streamsize = 0;
                }
            }
            if (gp_fseek(pdf_dev->xref_stream.file, 0, SEEK_SET) != 0)
                return_error(gs_error_ioerror);

            pprintd1(pdf_dev->strm, "trailer\n<<\n/Size %d\n/Root 1 0 R\n/ID [", pdf_dev->NextObject);
            pdf_compute_fileID(pdf_dev, fileID, CreationDate, Title, Producer);
            write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
            write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
            pprintd1(pdf_dev->strm, "]\n>>\nstartxref\n%d\n%%%%EOF\n", pdf_dev->xrefOffset);
        }

        sflush(pdf_dev->strm);
        pdf_dev->strm->file = NULL; /* Don't close underlying file when we free the stream */
        gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm, "pdfimage_close(strm)");
        pdf_dev->strm = 0;
        gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm_buf, "pdfimage_close(strmbuf)");
        pdf_dev->strm_buf = 0;
    }
    if (pdf_dev->Pages) {
        pdfimage_page *p = pdf_dev->Pages, *n;
        do {
            n = p->next;
            gs_free_object(pdf_dev->memory->non_gc_memory, p,
                           "pdfimage free a page");
            p = n;
        }while (p);
        pdf_dev->Pages = NULL;
        pdf_dev->NumPages = 0;
    }
    gsicc_free_link_dev(pdf_dev->icclink);
    pdf_dev->icclink = NULL;
    pdf_dev->RootOffset = 0;
    pdf_dev->PagesOffset = 0;
    pdf_dev->xrefOffset = 0;
    if (!PCLm)
        pdf_dev->StripHeight = 0;
    else
        pdf_dev->NextObject = 0;
    return 0;
}

int
pdf_image_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int code;

    code = gdev_pdf_image_begin_page(pdf_dev, file);
    if (code < 0)
        return code;

    code = pdf_image_downscale_and_print_page(pdev,
                                              &pdf_dev->downscale,
                                              8,
                                              pdf_dev->color_info.num_components);
    if (code < 0)
        return code;

    code = gx_parse_output_file_name(&parsed, &fmt, pdev->fname,
                                         strlen(pdev->fname), pdev->memory);

    if (code >= 0 && fmt) {
        code = pdf_image_finish_file(pdf_dev, 0);
    }
    return code;
}

int
pdf_image_open(gx_device *pdev)
{
    gx_device_pdf_image *ppdev;
    int code;
    bool update_procs = false;

    code = install_internal_subclass_devices((gx_device **)&pdev, &update_procs);
    if (code < 0)
        return code;
    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;

    ppdev = (gx_device_pdf_image *)pdev;
    ppdev->file = NULL;
    ppdev->Pages = NULL;
    ppdev->NumPages = 0;
    ppdev->RootOffset = 0;
    ppdev->PagesOffset = 0;
    ppdev->xrefOffset = 0;
    ppdev->StripHeight = 0;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (code < 0)
        return code;
    if (update_procs) {
        if (pdev->ObjectHandlerPushed) {
            gx_copy_device_procs(pdev->parent, pdev, &gs_obj_filter_device);
            pdev = pdev->parent;
        }
        if (pdev->PageHandlerPushed)
            gx_copy_device_procs(pdev->parent, pdev, &gs_flp_device);
    }
    if (ppdev->OpenOutputFile)
        code = gdev_prn_open_printer(pdev, 1);

    return code;
}

int
pdf_image_close(gx_device * pdev)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    int code;

    code = pdf_image_finish_file(pdf_dev, 0);
    if (code < 0)
        return code;

    return gdev_prn_close(pdev);
}

static int
pdf_image_compression_param_string(gs_param_string *param, unsigned char id)
{
    struct compression_string *c;
    for (c = compression_strings; c->str; c++)
        if (id == c->id) {
            param_string_from_string(*param, c->str);
            return 0;
        }
    return_error(gs_error_undefined);
}

static int
pdf_image_compression_id(unsigned char *id, gs_param_string *param)
{
    struct compression_string *c;
    for (c = compression_strings; c->str; c++)
        if (!bytes_compare(param->data, param->size,
                           (const byte *)c->str, strlen(c->str)))
        {
            *id = c->id;
            return 0;
        }
    return_error(gs_error_undefined);
}

static int
pdf_image_get_some_params(gx_device * dev, gs_param_list * plist, int which)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int code = gdev_prn_get_params(dev, plist);
    int ecode = code;
    gs_param_string comprstr;

    if (ecode < 0)
        return ecode;

    ecode = param_write_bool(plist, "Tumble", &pdf_dev->Tumble);
    if (ecode < 0)
        return ecode;
    ecode = param_write_bool(plist, "Tumble2", &pdf_dev->Tumble2);
    if (ecode < 0)
        return ecode;
    ecode = param_write_int(plist, "StripHeight", &pdf_dev->StripHeight);
    if (ecode < 0)
        return ecode;
    ecode = param_write_int(plist, "JPEGQ", &pdf_dev->JPEGQ);
    if (ecode < 0)
        return ecode;
    ecode = param_write_float(plist, "QFactor", &pdf_dev->QFactor);
    if (ecode < 0)
        return ecode;

    if ((code = pdf_image_compression_param_string(&comprstr, pdf_dev->Compression)) < 0 ||
        (code = param_write_string(plist, "Compression", &comprstr)) < 0)
        ecode = code;
    if (which & 1) {
        if ((code = gx_downscaler_write_params(plist, &pdf_dev->downscale,
                                               GX_DOWNSCALER_PARAMS_MFS |
                                               (which & 2 ? GX_DOWNSCALER_PARAMS_TRAP : 0) |
                                               (which & 4 ? GX_DOWNSCALER_PARAMS_ETS : 0))) < 0)
            ecode = code;
    }
    return ecode;
}

int
pdf_image_get_params_downscale(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_get_some_params(dev, plist, 1);
}

int
pdf_image_get_params_downscale_cmyk(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_get_some_params(dev, plist, 3);
}

int
pdf_image_get_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_get_some_params(dev, plist, 7);
}

static int
pdf_image_put_some_params(gx_device * dev, gs_param_list * plist, int which)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int ecode = 0;
    int code;
    unsigned char compr = pdf_dev->Compression;
    gs_param_string comprstr;
    const char *param_name;

    code = param_read_bool(plist, param_name = "Tumble", &pdf_dev->Tumble);
    if (code < 0) {
        errprintf(pdf_dev->memory, "Invalid Tumble setting\n");
        param_signal_error(plist, param_name, ecode);
        return code;
    }
    code = param_read_bool(plist, param_name = "Tumble2", &pdf_dev->Tumble2);
    if (code < 0) {
        errprintf(pdf_dev->memory, "Invalid Tumble2 setting\n");
        param_signal_error(plist, param_name, ecode);
        return code;
    }
    code = param_read_int(plist, param_name = "StripHeight", &pdf_dev->StripHeight);
    if (code < 0) {
        errprintf(pdf_dev->memory, "Invalid StripHeight setting\n");
        param_signal_error(plist, param_name, ecode);
        return code;
    }
    code = param_read_int(plist, param_name = "JPEGQ", &pdf_dev->JPEGQ);
    if (code < 0) {
        errprintf(pdf_dev->memory, "Invalid JPEQG setting\n");
        param_signal_error(plist, param_name, ecode);
        return code;
    }
    code = param_read_float(plist, param_name = "QFactor", &pdf_dev->QFactor);
    if (code < 0) {
        errprintf(pdf_dev->memory, "Invalid QFactor setting\n");
        param_signal_error(plist, param_name, ecode);
        return code;
    }
    /* Read Compression */
    switch (code = param_read_string(plist, (param_name = "Compression"), &comprstr)) {
        case 0:
            if ((ecode = pdf_image_compression_id(&compr, &comprstr)) < 0) {

                errprintf(pdf_dev->memory, "Unknown compression setting\n");
                param_signal_error(plist, param_name, ecode);
                return ecode;
            }
            pdf_dev->Compression = compr;
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }
    if (which & 1)
    {
        code = gx_downscaler_read_params(plist, &pdf_dev->downscale,
                                         (GX_DOWNSCALER_PARAMS_MFS |
                                          (which & 2 ? GX_DOWNSCALER_PARAMS_TRAP : 0) |
                                          (which & 4 ? GX_DOWNSCALER_PARAMS_ETS : 0)));
        if (code < 0)
            ecode = code;
    }
    if (ecode < 0)
        return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        return code;

    return code;
}

int
pdf_image_put_params_downscale(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_put_some_params(dev, plist, 1);
}

int
pdf_image_put_params_downscale_cmyk(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_put_some_params(dev, plist, 3);
}

int
pdf_image_put_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_put_some_params(dev, plist, 7);
}

static void
PCLm_get_initial_matrix(gx_device * dev, register gs_matrix * pmat)
{
    gx_device_pdf_image *pdev = (gx_device_pdf_image *)dev;

    gx_default_get_initial_matrix(dev, pmat);

    if (pdev->Duplex && (pdev->NumPages & 1)) {
        /* Duplexing and we are on the back of the page. */
        if (pdev->Tumble) {
            if (pdev->Tumble2) {
                /* Rotate by 180 degrees and flip on X. */
                pmat->xx = pmat->xx;
                pmat->xy = -pmat->xy;
                pmat->yx = -pmat->yx;
                pmat->yy = -pmat->yy;
                pmat->ty = pdev->height - pmat->ty;
            } else {
                /* Rotate by 180 degrees */
                pmat->xx = -pmat->xx;
                pmat->xy = -pmat->xy;
                pmat->yx = -pmat->yx;
                pmat->yy = -pmat->yy;
                pmat->tx = pdev->width - pmat->tx;
                pmat->ty = pdev->height - pmat->ty;
            }
        } else if (pdev->Tumble2) {
            /* Flip on X */
            pmat->xx = -pmat->xx;
            pmat->yx = -pmat->yx;
            pmat->tx = pdev->width - pmat->tx;
        }
    }
}

/* ------ The PCLm device ------ */
static void
PCLm_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, open_device, PCLm_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page);
    set_dev_proc(dev, get_initial_matrix, PCLm_get_initial_matrix);
    set_dev_proc(dev, close_device, PCLm_close);
    set_dev_proc(dev, get_params, pdf_image_get_params_downscale);
    set_dev_proc(dev, put_params, pdf_image_put_params_downscale);
}

static void
PCLm8_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, open_device, PCLm_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page);
    set_dev_proc(dev, get_initial_matrix, PCLm_get_initial_matrix);
    set_dev_proc(dev, close_device, PCLm_close);
    set_dev_proc(dev, get_params, pdf_image_get_params_downscale);
    set_dev_proc(dev, put_params, pdf_image_put_params_downscale);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

static dev_proc_print_page(PCLm_print_page);

const gx_device_pdf_image gs_PCLm_device = {
    prn_device_body_duplex(gx_device_pdf_image,
                           PCLm_initialize_device_procs,
                           "pclm",
                           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                           600, 600,   /* 600 dpi by default */
                           0, 0, 0, 0, /* Margins */
                           3,          /* num components */
                           24,         /* bits per sample */
                           255, 255, 256, 256,
                           PCLm_print_page),
    3,
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    16, /* StripHeight */
    0.0, /* QFactor */
    0  /* JPEGQ */
};

const gx_device_pdf_image gs_PCLm8_device = {
    prn_device_body_duplex(gx_device_pdf_image,
                           PCLm8_initialize_device_procs,
                           "pclm8",
                           DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
                           600, 600,   /* 600 dpi by default */
                           0, 0, 0, 0, /* Margins */
                           1,          /* num components */
                           8,         /* bits per sample */
                           255, 0, 256, 0,
                           PCLm_print_page),
    3,
    GX_DOWNSCALER_PARAMS_DEFAULTS,
    16, /* StripHeight */
    0.0, /* QFactor */
    0  /* JPEGQ */
};

/* Open a temporary file, with or without a stream. */
static int
PCLm_open_temp_file(gx_device_pdf_image *pdev, PCLm_temp_file_t *ptf)
{
    char fmode[4];

    if (strlen(gp_fmode_binary_suffix) > 2)
        return_error(gs_error_invalidfileaccess);

    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    ptf->file =	gp_open_scratch_file(pdev->memory,
                                     gp_scratch_file_name_prefix,
                                     ptf->file_name,
                                     fmode);
    if (ptf->file == 0)
        return_error(gs_error_invalidfileaccess);
    return 0;
}
/* Close and remove temporary files. */
static int
PCLm_close_temp_file(gx_device_pdf_image *pdev, PCLm_temp_file_t *ptf, int code)
{
    int err = 0;
    gp_file *file = ptf->file;

    /*
     * ptf->strm == 0 or ptf->file == 0 is only possible if this procedure
     * is called to clean up during initialization failure, but ptf->strm
     * might not be open if it was finalized before the device was closed.
     */
    if (ptf->strm) {
        if (s_is_valid(ptf->strm)) {
            sflush(ptf->strm);
            /* Prevent freeing the stream from closing the file. */
            ptf->strm->file = 0;
        } else
            ptf->file = file = 0;	/* file was closed by finalization */
        gs_free_object(pdev->memory->non_gc_memory, ptf->strm_buf,
                       "pdf_close_temp_file(strm_buf)");
        ptf->strm_buf = 0;
        gs_free_object(pdev->memory->non_gc_memory, ptf->strm,
                       "pdf_close_temp_file(strm)");
        ptf->strm = 0;
    }
    if (file) {
        err = gp_ferror(file) | gp_fclose(file);
        gp_unlink(pdev->memory, ptf->file_name);
        ptf->file = 0;
    }
    return
        (code < 0 ? code : err != 0 ? gs_note_error(gs_error_ioerror) : code);
}

static int
PCLm_open_temp_stream(gx_device_pdf_image *pdev, PCLm_temp_file_t *ptf)
{
    int code = PCLm_open_temp_file(pdev, ptf);

    if (code < 0)
        return code;
    ptf->strm = s_alloc(pdev->memory->non_gc_memory, "pdf_open_temp_stream(strm)");
    if (ptf->strm == 0) {
        PCLm_close_temp_file(pdev, ptf, 0);
        return_error(gs_error_VMerror);
    }
    ptf->strm_buf = gs_alloc_bytes(pdev->memory->non_gc_memory, 512,
                                   "pdf_open_temp_stream(strm_buf)");
    if (ptf->strm_buf == 0) {
        gs_free_object(pdev->memory->non_gc_memory, ptf->strm,
                       "pdf_open_temp_stream(strm)");
        ptf->strm = 0;
        PCLm_close_temp_file(pdev, ptf, 0);
        return_error(gs_error_VMerror);
    }
    swrite_file(ptf->strm, ptf->file, ptf->strm_buf, 512);
    return 0;
}
int
PCLm_open(gx_device *pdev)
{
    gx_device_pdf_image *ppdev;
    int code;
    bool update_procs = false;

    code = install_internal_subclass_devices((gx_device **)&pdev, &update_procs);
    if (code < 0)
        return code;
    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;

    ppdev = (gx_device_pdf_image *)pdev;
    memset(&ppdev->ocr, 0, sizeof(ppdev->ocr));
    ppdev->file = NULL;
    ppdev->Pages = NULL;
    ppdev->NumPages = 0;
    ppdev->RootOffset = 0;
    ppdev->PagesOffset = 0;
    ppdev->xrefOffset = 0;
    ppdev->NextObject = 0;
    code = gdev_prn_allocate_memory(pdev, NULL, 0, 0);
    if (code < 0)
        return code;
    if (update_procs) {
        if (pdev->ObjectHandlerPushed) {
            gx_copy_device_procs(pdev->parent, pdev, &gs_obj_filter_device);
            pdev = pdev->parent;
        }
        if (pdev->PageHandlerPushed)
            gx_copy_device_procs(pdev->parent, pdev, &gs_flp_device);
    }
    if (ppdev->OpenOutputFile)
        code = gdev_prn_open_printer(pdev, 1);

    if(code < 0)
        return code;

    code = PCLm_open_temp_stream(ppdev, &ppdev->xref_stream);
    if (code < 0)
        return code;

    code = PCLm_open_temp_stream(ppdev, &ppdev->temp_stream);
    if (code < 0)
        PCLm_close_temp_file(ppdev, &ppdev->xref_stream, 0);
    return code;
}

static int gdev_PCLm_begin_page(gx_device_pdf_image *pdf_dev,
                         gp_file *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)pdf_dev;
    int code;
    pdfimage_page *page;

    page = (pdfimage_page *)gs_alloc_bytes(pdf_dev->memory->non_gc_memory, sizeof(pdfimage_page), "pdfimage create new page");
    if (page == NULL)
        return_error(gs_error_VMerror);

    memset(page, 0x00, sizeof(pdfimage_page));

    if (gdev_prn_file_is_new(pdev)) {
        /* Set up the icc link settings at this time */
        code = gx_downscaler_create_post_render_link((gx_device *)pdev,
                                                     &pdf_dev->icclink);
        if (code < 0) {
            gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
            return code;
        }

        /* Set up the stream and insert the file header */
        pdf_dev->strm = s_alloc(pdf_dev->memory->non_gc_memory, "pdfimage_open_temp_stream(strm)");
        if (pdf_dev->strm == 0) {
            gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
            return_error(gs_error_VMerror);
        }
        pdf_dev->strm_buf = gs_alloc_bytes(pdf_dev->memory->non_gc_memory, 512,
                                       "pdfimage_open_temp_stream(strm_buf)");
        if (pdf_dev->strm_buf == 0) {
            pdf_dev->strm->file = NULL; /* Don't close underlying file when we free the stream */
            gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm,
                           "pdfimage_open_temp_stream(strm)");
            pdf_dev->strm = 0;
            gs_free_object(pdf_dev->memory->non_gc_memory, page, "pdfimage create new page");
            return_error(gs_error_VMerror);
        }
        swrite_file(pdf_dev->strm, pdf_dev->file, pdf_dev->strm_buf, 512);

        stream_puts(pdf_dev->strm, "%PDF-1.3\n");
        stream_puts(pdf_dev->strm, "%PCLm 1.0\n");
        pdf_dev->Pages = page;
        pdf_dev->NextObject = 4;
    } else {
        pdfimage_page *current = pdf_dev->Pages;
        while(current->next)
            current = current->next;
        current->next = page;
    }
    page->PageDictObjectNumber = pdf_dev->NextObject++;
    page->PageStreamObjectNumber = pdf_dev->NextObject++;
    page->ImageObjectNumber = pdf_dev->NextObject++;

    return 0;
}

static int
PCLm_downscale_and_print_page(gx_device_printer *dev,
                              int bpc, int num_comps)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int code = 0;
    uint Read;
    byte *data = NULL;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size = size;
    int row, NumStrips;
    double StripDecrement;
    const gx_downscaler_params *params = &pdf_dev->downscale;
    int factor = params->downscale_factor;
    int height = dev->height/factor;
    int width = dev->width/factor;
    gx_downscaler_t ds;
    gs_offset_t stream_pos = 0;
    pdfimage_page *page = pdf_dev->Pages;
    char Buffer[1024];

    if (page == NULL)
        return_error(gs_error_undefined);

    while(page->next)
        page = page->next;

    if (pdf_dev->icclink == NULL) {
        code = gx_downscaler_init(&ds, (gx_device *)dev,
                                  8, bpc, num_comps,
                                  params,
                                  NULL, /*&fax_adjusted_width*/
                                  0 /*aw*/);
    } else {
        code = gx_downscaler_init_cm(&ds, (gx_device *)dev,
                                     8, bpc, num_comps,
                                     params,
                                     NULL, /*&fax_adjusted_width*/
                                     0, /*aw*/
                                     pdf_image_chunky_post_cm,
                                     pdf_dev->icclink,
                                     pdf_dev->icclink->num_output);
    }
    if (code < 0)
        return code;

    data = gs_alloc_bytes(dev->memory, max_size, "pdf_image_print_page(data)");
    if (data == NULL) {
        gx_downscaler_fin(&ds);
        return_error(gs_error_VMerror);
    }

    if (pdf_dev->StripHeight == 0) {
        NumStrips = 1;
        pdf_dev->StripHeight = pdf_dev->height;
    }
    else
        NumStrips = (int)ceil((float)height / pdf_dev->StripHeight);

    page->PageDictOffset = stell(pdf_dev->strm);
    write_xref_entry(pdf_dev->xref_stream.strm, page->PageDictOffset);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageDictObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Contents %d 0 R\n", page->PageStreamObjectNumber);
    stream_puts(pdf_dev->strm, "/Type /Page\n/Parent 2 0 R\n");
    gs_snprintf(Buffer, sizeof(Buffer), "/MediaBox [0 0 %.3f %.3f]\n", ((double)pdf_dev->width / pdf_dev->HWResolution[0]) * 72, ((double)pdf_dev->height / pdf_dev->HWResolution[1]) * 72);
    stream_puts(pdf_dev->strm, Buffer);
    stream_puts(pdf_dev->strm, "/Resources <<\n/XObject <<\n");

    if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
        gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
        gx_downscaler_fin(&ds);
        return_error(gs_error_ioerror);
    }

    StripDecrement = pdf_dev->StripHeight / (pdf_dev->HWResolution[1] / (factor * 72));

    for (row = 0; row < NumStrips;row++) {
        stream_puts(pdf_dev->temp_stream.strm, "/P <</MCID 0>> BDC q\n");
        /* Special handling for the last strip, potentially shorter than all the previous ones */
        if (row == NumStrips - 1) {
            double adjusted;
            adjusted = height - (row * pdf_dev->StripHeight);
            adjusted = adjusted / (pdf_dev->HWResolution[1] / (factor * 72));
            gs_snprintf(Buffer, sizeof(Buffer), "%.3f 0 0 %.3f 0 0 cm\n/Im%d Do Q\n", (width / (pdf_dev->HWResolution[0] / 72)) * factor, adjusted, row);
        } else
            gs_snprintf(Buffer, sizeof(Buffer), "%.3f 0 0 %.3f 0 %f cm\n/Im%d Do Q\n", (width / (pdf_dev->HWResolution[0] / 72)) * factor, StripDecrement, ((height / (pdf_dev->HWResolution[1] / 72)) * factor) - (StripDecrement * (row + 1)), row);
        stream_puts(pdf_dev->temp_stream.strm, Buffer);
        pprintd2(pdf_dev->strm, "/Im%d %d 0 R\n", row, page->ImageObjectNumber + (row * 2));
    }
    sflush(pdf_dev->temp_stream.strm);
    stream_pos = gp_ftell(pdf_dev->temp_stream.file);
    stream_puts(pdf_dev->strm, ">>\n>>\n>>\nendobj\n");

    page->PageStreamOffset = stell(pdf_dev->strm);
    write_xref_entry(pdf_dev->xref_stream.strm, page->PageStreamOffset);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageStreamObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Length %d\n>>\nstream\n", stream_pos);

    if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
        gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
        gx_downscaler_fin(&ds);
        return_error(gs_error_ioerror);
    }

    Read = 0;
    while(stream_pos > 0) {
        if (stream_pos > 1024) {
            stream_pos -= gp_fpread(Buffer, 1024, Read, pdf_dev->temp_stream.file);
            Read += 1024;
            stream_write(pdf_dev->strm, Buffer, 1024);
        }
        else {
            gp_fpread(Buffer, stream_pos, Read, pdf_dev->temp_stream.file);
            stream_write(pdf_dev->strm, Buffer, stream_pos);
            stream_pos = 0;
        }
    }

    stream_puts(pdf_dev->strm, "endstream\nendobj\n");

    if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
        gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
        gx_downscaler_fin(&ds);
        return_error(gs_error_ioerror);
    }

    pdf_dev->temp_stream.save = pdf_dev->temp_stream.strm;
    switch (pdf_dev->Compression) {
        case COMPRESSION_FLATE:
            encode((gx_device *)pdf_dev, &pdf_dev->temp_stream.strm, &s_zlibE_template, pdf_dev->memory->non_gc_memory);
            break;
        case COMPRESSION_JPEG:
            encode((gx_device *)pdf_dev, &pdf_dev->temp_stream.strm, &s_DCTE_template, pdf_dev->memory->non_gc_memory);
            break;
        case COMPRESSION_RLE:
            encode((gx_device *)pdf_dev, &pdf_dev->temp_stream.strm, &s_RLE_template, pdf_dev->memory->non_gc_memory);
            break;
        default:
            break;
    }

    Read = 0;
    for (row = 0; row < height;row++) {
        code = gx_downscaler_getbits(&ds, data, row);
        if (code < 0)
            break;
        stream_write(pdf_dev->temp_stream.strm, data, width * num_comps);
        Read++;
        if (Read == pdf_dev->StripHeight) {
            uint R = 0;

            if (pdf_dev->temp_stream.save != pdf_dev->temp_stream.strm)
                s_close_filters(&pdf_dev->temp_stream.strm, pdf_dev->temp_stream.save);
            sflush(pdf_dev->temp_stream.strm);
            stream_pos = gp_ftell(pdf_dev->temp_stream.file);

            page->ImageOffset = stell(pdf_dev->strm);
            write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
            pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
            pprintd1(pdf_dev->strm, "<<\n/Length %d\n", stream_pos);
            stream_puts(pdf_dev->strm, "/Subtype /Image\n");
            pprintd1(pdf_dev->strm, "/Width %d\n", width);
            pprintd1(pdf_dev->strm, "/Height %d\n", Read);
            if (dev->color_info.max_components == 1)
                stream_puts(pdf_dev->strm, "/ColorSpace /DeviceGray\n");
            else
                stream_puts(pdf_dev->strm, "/ColorSpace /DeviceRGB\n");
            stream_puts(pdf_dev->strm, "/BitsPerComponent 8\n");
            switch (pdf_dev->Compression) {
                case COMPRESSION_FLATE:
                    stream_puts(pdf_dev->strm, "/Filter /FlateDecode\n");
                    stream_puts(pdf_dev->strm, ">>\nstream\n");
                    break;
                case COMPRESSION_JPEG:
                    stream_puts(pdf_dev->strm, "/Filter /DCTDecode\n");
                    stream_puts(pdf_dev->strm, ">>\nstream\n");
                    break;
                case COMPRESSION_RLE:
                    stream_puts(pdf_dev->strm, "/Filter /RunLengthDecode\n");
                    stream_puts(pdf_dev->strm, ">>\nstream\n");
                    break;
                default:
                case COMPRESSION_LZW:
                case COMPRESSION_NONE:
                    stream_puts(pdf_dev->strm, ">>\nstream\n");
                    break;
            }
            if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
                gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
                gx_downscaler_fin(&ds);
                return_error(gs_error_ioerror);
            }

            while(stream_pos > 0) {
                if (stream_pos > 1024) {
                    stream_pos -= gp_fpread(Buffer, 1024, R, pdf_dev->temp_stream.file);
                    R += 1024;
                    stream_write(pdf_dev->strm, Buffer, 1024);
                }
                else {
                    memset(Buffer, 0xf5, 1024);
                    gp_fpread(Buffer, stream_pos, R, pdf_dev->temp_stream.file);
                    stream_write(pdf_dev->strm, Buffer, stream_pos);
                    stream_pos = 0;
                }
            }

            stream_puts(pdf_dev->strm, "\nendstream\nendobj\n");

            if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
                gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
                gx_downscaler_fin(&ds);
                return_error(gs_error_ioerror);
            }

            switch (pdf_dev->Compression) {
                case COMPRESSION_FLATE:
                    encode((gx_device *)pdf_dev, &pdf_dev->temp_stream.strm, &s_zlibE_template, pdf_dev->memory->non_gc_memory);
                    break;
                case COMPRESSION_JPEG:
                    {
                        int t = pdf_dev->StripHeight;

                        if (height - (row + 1) < pdf_dev->StripHeight)
                            pdf_dev->StripHeight = height - (row + 1);
                        encode((gx_device *)pdf_dev, &pdf_dev->temp_stream.strm, &s_DCTE_template, pdf_dev->memory->non_gc_memory);
                        pdf_dev->StripHeight = t;
                    }
                    break;
                case COMPRESSION_RLE:
                    encode((gx_device *)pdf_dev, &pdf_dev->temp_stream.strm, &s_RLE_template, pdf_dev->memory->non_gc_memory);
                    break;
                default:
                    break;
            }
            Read = 0;

            page->ImageOffset = stell(pdf_dev->strm);
            write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
            pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
            stream_puts(pdf_dev->strm, "<</Length 14>>\nstream\nq /image Do Q\nendstream\nendobj\n");
        }
    }

    if (code < 0) {
        gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
        gx_downscaler_fin(&ds);
        return code;
    }

    if (Read) {
        uint R = 0, saved = pdf_dev->StripHeight;

        pdf_dev->StripHeight = Read;
        if (pdf_dev->temp_stream.save != pdf_dev->temp_stream.strm)
            s_close_filters(&pdf_dev->temp_stream.strm, pdf_dev->temp_stream.save);
        sflush(pdf_dev->temp_stream.strm);
        stream_pos = gp_ftell(pdf_dev->temp_stream.file);

        page->ImageOffset = stell(pdf_dev->strm);
        write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
        pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
        pprintd1(pdf_dev->strm, "<<\n/Length %d\n", stream_pos);
        stream_puts(pdf_dev->strm, "/Subtype /Image\n");
        pprintd1(pdf_dev->strm, "/Width %d\n", width);
        pprintd1(pdf_dev->strm, "/Height %d\n", Read);
        if (dev->color_info.max_components == 1)
            stream_puts(pdf_dev->strm, "/ColorSpace /DeviceGray\n");
        else
            stream_puts(pdf_dev->strm, "/ColorSpace /DeviceRGB\n");
        stream_puts(pdf_dev->strm, "/BitsPerComponent 8\n");
        switch (pdf_dev->Compression) {
            case COMPRESSION_FLATE:
                stream_puts(pdf_dev->strm, "/Filter /FlateDecode\n");
                stream_puts(pdf_dev->strm, ">>\nstream\n");
                break;
            default:
            case COMPRESSION_JPEG:
                stream_puts(pdf_dev->strm, "/Filter /DCTDecode\n");
                stream_puts(pdf_dev->strm, ">>\nstream\n");
                break;
            case COMPRESSION_RLE:
                stream_puts(pdf_dev->strm, "/Filter /RunLengthDecode\n");
                stream_puts(pdf_dev->strm, ">>\nstream\n");
                break;
            case COMPRESSION_LZW:
            case COMPRESSION_NONE:
                stream_puts(pdf_dev->strm, ">>\nstream\n");
                break;
        }
        if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
            gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
            gx_downscaler_fin(&ds);
            return_error(gs_error_ioerror);
        }

        while(stream_pos > 0) {
            if (stream_pos > 1024) {
                stream_pos -= gp_fpread(Buffer, 1024, R, pdf_dev->temp_stream.file);
                R += 1024;
                stream_write(pdf_dev->strm, Buffer, 1024);
            }
            else {
                gp_fpread(Buffer, stream_pos, R, pdf_dev->temp_stream.file);
                stream_write(pdf_dev->strm, Buffer, stream_pos);
                stream_pos = 0;
            }
        }

        stream_puts(pdf_dev->strm, "\nendstream\nendobj\n");

        if (gp_fseek(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
            gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
            gx_downscaler_fin(&ds);
            return_error(gs_error_ioerror);
        }
        page->ImageOffset = stell(pdf_dev->strm);
        write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
        pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
        stream_puts(pdf_dev->strm, "<</Length 14>>\nstream\nq /image Do Q\nendstream\nendobj\n");
        pdf_dev->StripHeight = saved;
    }
    pdf_dev->NextObject = page->ImageObjectNumber;

    gx_downscaler_fin(&ds);
    gs_free_object(dev->memory, data, "pdf_image_print_page(data)");

    pdf_dev->NumPages++;
    return code;
}

static int
PCLm_print_page(gx_device_printer * pdev, gp_file * file)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    const char *fmt;
    gs_parsed_file_name_t parsed;
    int code;

    code = gdev_PCLm_begin_page(pdf_dev, file);
    if (code < 0)
        return code;

    code = PCLm_downscale_and_print_page(pdev,
                                         8, pdf_dev->color_info.num_components);
    if (code < 0)
        return code;

    code = gx_parse_output_file_name(&parsed, &fmt, pdev->fname,
                                         strlen(pdev->fname), pdev->memory);

    if (code >= 0 && fmt) {
        code = pdf_image_finish_file(pdf_dev, 1);
    }
    return code;
}

int
PCLm_close(gx_device * pdev)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    int code, code1;

    code = pdf_image_finish_file(pdf_dev, 1);
    if (code < 0)
        return code;

    code = PCLm_close_temp_file(pdf_dev, &pdf_dev->xref_stream, 0);
    code1 = PCLm_close_temp_file(pdf_dev, &pdf_dev->temp_stream, 0);
    if (code == 0)
        code = code1;
    code1 = gdev_prn_close(pdev);
    if (code == 0)
        code = code1;
    return code;
}
