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
#include "jpeglib.h"
#include "sdct.h"
#include "srlx.h"
#include "gsicc_cache.h"
#include "sjpeg.h"

#define	    COMPRESSION_NONE	1	/* dump mode */
#define	    COMPRESSION_LZW		2       /* Lempel-Ziv  & Welch */
#define	    COMPRESSION_FLATE	3
#define	    COMPRESSION_JPEG	4
#define	    COMPRESSION_RLE		5

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

int pdf_image_open(gx_device *pdev);
int PCLm_open(gx_device *pdev);
int pdf_image_close(gx_device * pdev);
int PCLm_close(gx_device * pdev);
int pdf_image_put_params_downscale(gx_device * dev, gs_param_list * plist);
int pdf_image_put_params_downscale_cmyk(gx_device * dev, gs_param_list * plist);
int pdf_image_put_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist);
int pdf_image_get_params_downscale(gx_device * dev, gs_param_list * plist);
int pdf_image_get_params_downscale_cmyk(gx_device * dev, gs_param_list * plist);
int pdf_image_get_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist);

typedef struct pdfimage_page_s {
    int ImageObjectNumber;
    gs_offset_t ImageOffset;int LengthObjectNumber;
    gs_offset_t LengthOffset;
    int PageStreamObjectNumber;
    gs_offset_t PageStreamOffset;
    int PageDictObjectNumber;
    gs_offset_t PageDictOffset;
    void *next;
} pdfimage_page;

typedef struct PCLm_temp_file_s {
    char file_name[gp_file_name_sizeof];
    FILE *file;
    stream *strm;
    stream *save;
    byte *strm_buf;
} PCLm_temp_file_t;

typedef struct gx_device_pdf_image_s {
    gx_device_common;
    gx_prn_device_common;
    unsigned char Compression;
    gx_downscaler_params downscale;
    int StripHeight;
    float QFactor;
    int JPEGQ;
    gsicc_link_t *icclink;
    stream *strm;
    byte *strm_buf;
    long uuid_time[2];
    int NumPages;
    gs_offset_t RootOffset;
    gs_offset_t PagesOffset;
    gs_offset_t MetadataOffset;
    gs_offset_t MetadataLengthOffset;
    gs_offset_t InfoOffset;
    gs_offset_t xrefOffset;
    pdfimage_page *Pages;
    PCLm_temp_file_t xref_stream;
    PCLm_temp_file_t temp_stream;
    int NextObject;
} gx_device_pdf_image;

static dev_proc_print_page(pdf_image_print_page);

/* ------ The pdfimage8 device ------ */

static const gx_device_procs pdfimage8_procs =
prn_color_params_procs(pdf_image_open,
                       gdev_prn_output_page_seekable,
                       pdf_image_close,
                       gx_default_gray_map_rgb_color,
                       gx_default_gray_map_color_rgb,
                       pdf_image_get_params_downscale,
                       pdf_image_put_params_downscale);

const gx_device_pdf_image gs_pdfimage8_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfimage8_procs,
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

static const gx_device_procs pdfimage24_procs =
prn_color_params_procs(pdf_image_open,
                       gdev_prn_output_page_seekable,
                       pdf_image_close,
                       gx_default_rgb_map_rgb_color,
                       gx_default_rgb_map_color_rgb,
                       pdf_image_get_params_downscale,
                       pdf_image_put_params_downscale);

const gx_device_pdf_image gs_pdfimage24_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfimage24_procs,
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

static const gx_device_procs pdfimage32_procs = {
    pdf_image_open, NULL, NULL, gdev_prn_output_page_seekable, pdf_image_close,
    NULL, cmyk_8bit_map_color_cmyk, NULL, NULL, NULL, NULL, NULL, NULL,
    pdf_image_get_params_downscale_cmyk, pdf_image_put_params_downscale_cmyk,
    cmyk_8bit_map_cmyk_color, NULL, NULL, NULL, gx_page_device_get_page_device
};

const gx_device_pdf_image gs_pdfimage32_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfimage32_procs,
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

static int gdev_pdf_image_begin_page(gx_device_pdf_image *pdf_dev,
                         FILE *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)pdf_dev;
    cmm_dev_profile_t *profile_struct;
    gsicc_rendering_param_t rendering_params;
    int code;
    pdfimage_page *page;

    page = (pdfimage_page *)gs_alloc_bytes(pdf_dev->memory->non_gc_memory, sizeof(pdfimage_page), "pdfimage create new page");
    if (page == NULL)
        return_error(gs_error_VMerror);

    memset(page, 0x00, sizeof(pdfimage_page));

    if (gdev_prn_file_is_new(pdev)) {
        /* Set up the icc link settings at this time */
        code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);
        if (code < 0)
            return_error(gs_error_undefined);
        if (profile_struct->postren_profile != NULL) {
            rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
            rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;
            rendering_params.override_icc = false;
            rendering_params.preserve_black = gsBLACKPRESERVE_OFF;
            rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
            rendering_params.cmm = gsCMM_DEFAULT;
            if (profile_struct->oi_profile != NULL) {
                pdf_dev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->oi_profile, profile_struct->postren_profile,
                    &rendering_params);
            } else if (profile_struct->link_profile != NULL) {
                pdf_dev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->link_profile, profile_struct->postren_profile,
                    &rendering_params);
            } else {
                pdf_dev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->device_profile[0], profile_struct->postren_profile,
                    &rendering_params);
            }
            /* If it is identity, release it now and set link to NULL */
            if (pdf_dev->icclink->is_identity) {
                pdf_dev->icclink->procs.free_link(pdf_dev->icclink);
                gsicc_free_link_dev(pdev->memory, pdf_dev->icclink);
                pdf_dev->icclink = NULL;
            }
        }

        /* Set up the stream and insert the file header */
        pdf_dev->strm = s_alloc(pdf_dev->memory->non_gc_memory, "pdfimage_open_temp_stream(strm)");
        if (pdf_dev->strm == 0)
            return_error(gs_error_VMerror);
        pdf_dev->strm_buf = gs_alloc_bytes(pdf_dev->memory->non_gc_memory, pdf_dev->width * (pdf_dev->color_info.depth / 8),
                                       "pdfimage_open_temp_stream(strm_buf)");
        if (pdf_dev->strm_buf == 0) {
            gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm,
                           "pdfimage_open_temp_stream(strm)");
            pdf_dev->strm = 0;
            return_error(gs_error_VMerror);
        }
        swrite_file(pdf_dev->strm, pdf_dev->file, pdf_dev->strm_buf, pdf_dev->width * (pdf_dev->color_info.depth / 8));

        stream_puts(pdf_dev->strm, "%PDF-1.3\n");
        stream_puts(pdf_dev->strm, "%\307\354\217\242\n");
        pdf_dev->Pages = page;
    } else {
        pdfimage_page *current = pdf_dev->Pages;
        while(current->next)
            current = current->next;
        current->next = page;
    }
    page->ImageObjectNumber = (pdf_dev->NumPages * 4) + 6;
    page->LengthObjectNumber = page->ImageObjectNumber + 1;
    page->PageStreamObjectNumber = page->LengthObjectNumber + 1;
    page->PageDictObjectNumber = page->PageStreamObjectNumber + 1;
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
pdf_image_downscale_and_print_page(gx_device_printer *dev, int factor,
                              int mfs, int bpc, int num_comps,
                              int trap_w, int trap_h, const int *trap_order,
                              int ets)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int code = 0;
    byte *data = NULL;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size =size;
    int row;
    int height = dev->height/factor;
    int width = dev->width/factor;
    gx_downscaler_t ds;
    gs_offset_t stream_pos = 0;
    pdfimage_page *page = pdf_dev->Pages;
    char Buffer[1024];
    stream *s = pdf_dev->strm;

    if (page == NULL)
        return_error(gs_error_undefined);

    while(page->next)
        page = page->next;

    if (num_comps == 4) {
        if (pdf_dev->icclink == NULL) {
            code = gx_downscaler_init_trapped_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, NULL, /*&fax_adjusted_width*/ 0, /*aw*/ trap_w, trap_h, trap_order, ets);
        } else {
            code = gx_downscaler_init_trapped_cm_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, NULL, /*&fax_adjusted_width*/ 0, /*aw*/ trap_w, trap_h, trap_order,
                pdf_image_chunky_post_cm, pdf_dev->icclink, pdf_dev->icclink->num_output, ets);
        }
    } else {
        if (pdf_dev->icclink == NULL) {
            code = gx_downscaler_init_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, NULL, /*&fax_adjusted_width*/ 0, /*aw*/ ets);
        } else {
            code = gx_downscaler_init_cm_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
                factor, mfs, NULL, /*&fax_adjusted_width*/ 0, /*aw*/ pdf_image_chunky_post_cm, pdf_dev->icclink,
                pdf_dev->icclink->num_output, ets);
        }
    }
    if (code < 0)
        return code;

    data = gs_alloc_bytes(dev->memory, max_size, "pdf_image_print_page(data)");
    if (data == NULL) {
        gx_downscaler_fin(&ds);
        return_error(gs_error_VMerror);
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
        stream_write(pdf_dev->strm, data, width * num_comps);
    }

    if (code < 0) {
        gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
        gx_downscaler_fin(&ds);
        return code;;
    }

    stream_pos = stell(pdf_dev->strm) - stream_pos;

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
    pprintd1(pdf_dev->strm, "%d\n", stream_pos);
    stream_puts(pdf_dev->strm, "endobj\n");

    page->PageStreamOffset = stell(pdf_dev->strm);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageStreamObjectNumber);
    gs_sprintf(Buffer, "%f 0 0 %f 0 0 cm\n/Im1 Do", (width / (pdf_dev->HWResolution[0] / 72)) * factor, (height / (pdf_dev->HWResolution[1] / 72)) * factor);
    pprintd1(pdf_dev->strm, "<<\n/Length %d\n>>\nstream\n", strlen(Buffer));
    stream_puts(pdf_dev->strm, Buffer);
    stream_puts(pdf_dev->strm, "\nendstream\nendobj\n");

    page->PageDictOffset = stell(pdf_dev->strm);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageDictObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Contents %d 0 R\n", page->PageStreamObjectNumber);
    stream_puts(pdf_dev->strm, "/Type /Page\n/Parent 2 0 R\n");
    gs_sprintf(Buffer, "/MediaBox [0 0 %f %f]\n", ((double)pdf_dev->width / pdf_dev->HWResolution[0]) * 72, ((double)pdf_dev->height / pdf_dev->HWResolution[1]) * 72);
    stream_puts(pdf_dev->strm, Buffer);
    pprintd1(pdf_dev->strm, "/Resources <<\n/XObject <<\n/Im1 %d 0 R\n>>\n>>\n>>\n", page->ImageObjectNumber);
    stream_puts(pdf_dev->strm, "endobj\n");

    gx_downscaler_fin(&ds);
    gs_free_object(dev->memory, data, "pdf_image_print_page(data)");

    pdf_dev->NumPages++;
    return code;
}

static int
pdf_image_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    int code;

    code = gdev_pdf_image_begin_page(pdf_dev, file);
    if (code < 0)
        return code;

    code = pdf_image_downscale_and_print_page(pdev,
                                         pdf_dev->downscale.downscale_factor,
                                         pdf_dev->downscale.min_feature_size,
                                         8, pdf_dev->color_info.num_components,
                                         0, 0, NULL,
                                         pdf_dev->downscale.ets);
    if (code < 0)
        return code;

    return 0;
}

int
pdf_image_open(gx_device *pdev)
{
    gx_device_pdf_image *ppdev = (gx_device_pdf_image *)pdev;
    int code;
    bool update_procs = false;

    code = install_internal_subclass_devices((gx_device **)&pdev, &update_procs);
    if (code < 0)
        return code;
    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;

    ppdev->file = NULL;
    ppdev->Pages = NULL;
    ppdev->NumPages = 0;
    ppdev->RootOffset = 0;
    ppdev->PagesOffset = 0;
    ppdev->MetadataOffset = 0;
    ppdev->MetadataLengthOffset = 0;
    ppdev->InfoOffset = 0;
    ppdev->xrefOffset = 0;
    ppdev->StripHeight = 0;
    gp_get_realtime(ppdev->uuid_time);
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
        code = gdev_prn_open_printer_seekable(pdev, 1, true);
    return code;
}

static void write_xref_entry (stream *s, gs_offset_t Offset)
{
    char O[11];
    int i;

    if (Offset > 9999999999){
        Offset = 0;
    }
    gs_sprintf(O, "%d", Offset);
    for (i=0; i< (10 - strlen(O)); i++)
        stream_puts(s, "0");
    stream_puts(s, O);
    stream_puts(s, " 00000 n \n");
}

static uint64_t
pdf_uuid_time(void)
{
    long dt[2];
    uint64_t t;

    gp_get_realtime((long *)&dt);
    /* UUIDs use time in 100ns ticks since Oct 15, 1582. */
    t = (uint64_t)10000000 * dt[0] + dt[0] / 100; /* since Jan. 1, 1980 */
    t += (uint64_t) (1000*1000*10)         /* seconds */
       * (uint64_t) (60 * 60 * 24)         /* days */
       * (uint64_t) (17+30+31+365*397+99); /* # of days */
    return t;
}
static void writehex(char **p, ulong v, int l)
{
    int i = l * 2;
    static const char digit[] = "0123456789abcdef";

    for (; i--;)
        *((*p)++) = digit[v >> (i * 4) & 15];
}

static void
pdf_make_uuid(const byte node[6], uint64_t uuid_time, ulong time_seq, char *buf, int buf_length)
{
    char b[45], *p = b;
    ulong  uuid_time_lo = (ulong)(uuid_time & 0xFFFFFFFF);       /* MSVC 7.1.3088           */
    ushort uuid_time_md = (ushort)((uuid_time >> 32) & 0xFFFF);  /* cannot compile this     */
    ushort uuid_time_hi = (ushort)((uuid_time >> 48) & 0x0FFF);  /* as function arguments.  */

    writehex(&p, uuid_time_lo, 4); /* time_low */
    *(p++) = '-';
    writehex(&p, uuid_time_md, 2); /* time_mid */
    *(p++) = '-';
    writehex(&p, uuid_time_hi | (ushort)(1 << 12), 2); /* time_hi_and_version */
    *(p++) = '-';
    writehex(&p, (time_seq & 0x3F00) >> 8, 1); /* clock_seq_hi_and_reserved */
    writehex(&p, time_seq & 0xFF, 1); /* clock_seq & 0xFF */
    *(p++) = '-';
    writehex(&p, node[0], 1);
    writehex(&p, node[1], 1);
    writehex(&p, node[2], 1);
    writehex(&p, node[3], 1);
    writehex(&p, node[4], 1);
    writehex(&p, node[5], 1);
    *p = 0;
    strncpy(buf, b, buf_length);
}
static int
pdf_make_instance_uuid(const byte digest[6], char *buf, int buf_length)
{
    char URI_prefix[5] = "uuid:";

    memcpy(buf, URI_prefix, 5);
    pdf_make_uuid(digest, pdf_uuid_time(), 0, buf + 5, buf_length - 5);
    return 0;
}

static int
pdf_make_document_uuid(const byte digest[6], char *buf, int buf_length)
{
    char URI_prefix[5] = "uuid:";

    memcpy(buf, URI_prefix, 5);
    pdf_make_uuid(digest, pdf_uuid_time(), 0, buf+5, buf_length - 5);
    return 0;
}
static int write_xml(gx_device_pdf_image *pdev, stream *s, char *Title, char *Producer, char *Creator, struct tm *tms, char timesign, int timeoffset, byte digest[6])
{
    char instance_uuid[45], document_uuid[45];
    int code;

    code = pdf_make_instance_uuid(digest, instance_uuid, sizeof(instance_uuid));
    if (code < 0)
        return code;
    code = pdf_make_document_uuid(digest, document_uuid, sizeof(document_uuid));
    if (code < 0)
        return code;

    stream_puts(s, "<?xpacket begin='﻿' id='W5M0MpCehiHzreSzNTczkc9d'?>\n");
    stream_puts(s, "<?adobe-xap-filters esc=\"CRLF\"?>\n");
    stream_puts(s, "<x:xmpmeta xmlns:x='adobe:ns:meta/' x:xmptk='XMP toolkit 2.9.1-13, framework 1.6'>\n");
    stream_puts(s, "<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#' xmlns:iX='http://ns.adobe.com/iX/1.0/'>\n");
    stream_puts(s, "<rdf:Description rdf:about='");
    stream_puts(s, instance_uuid);
    stream_puts(s, "' xmlns:pdf='http://ns.adobe.com/pdf/1.3/' pdf:Producer='");
    stream_puts(s, Producer);
    stream_puts(s, "'/>\n");
    stream_puts(s, "<rdf:Description rdf:about='");
    stream_puts(s, instance_uuid);
    stream_puts(s, "' xmlns:xmp='http://ns.adobe.com/xap/1.0/'><xmp:ModifyDate>");
    pprintd3(s, "%d-%d-%dT", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday);
    pprintd3(s, "%d:%d:%dZ", tms->tm_hour, tms->tm_min, tms->tm_sec);
    stream_puts(s, "</xmp:ModifyDate>\n");
    stream_puts(s, "<xmp:CreateDate>");
    pprintd3(s, "%d-%d-%dT", tms->tm_year + 1900, tms->tm_mon + 1, tms->tm_mday);
    pprintd3(s, "%d:%d:%dZ", tms->tm_hour, tms->tm_min, tms->tm_sec);
    stream_puts(s, "</xmp:CreateDate>\n");
    stream_puts(s, "<xmp:CreatorTool>");
    stream_puts(s, Creator);
    stream_puts(s, "</xmp:CreatorTool></rdf:Description>\n");
    stream_puts(s, "<rdf:Description rdf:about='");
    stream_puts(s, instance_uuid);
    stream_puts(s, "' xmlns:xapMM='http://ns.adobe.com/xap/1.0/mm/' xapMM:DocumentID='");
    stream_puts(s, document_uuid);
    stream_puts(s, "'/>\n");
    stream_puts(s, "<rdf:Description rdf:about='");
    stream_puts(s, instance_uuid);
    stream_puts(s, "' xmlns:dc='http://purl.org/dc/elements/1.1/' dc:format='application/pdf'><dc:title><rdf:Alt><rdf:li xml:lang='x-default'>");
    stream_puts(s, Title);
    stream_puts(s, "</rdf:li></rdf:Alt></dc:title></rdf:Description>\n");
    stream_puts(s, "</rdf:RDF>\n");
    stream_puts(s, "</x:xmpmeta>\n");
    stream_puts(s, "                                                                        \n");
    stream_puts(s, "                                                                        \n");
    stream_puts(s, "<?xpacket end='w'?>\n");
    return 0;
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

    gp_get_usertime(secs_ns);
    sputs(s, (byte *)secs_ns, sizeof(secs_ns), &ignore);
    sputs(s, (const byte *)pdev->fname, strlen(pdev->fname), &ignore);

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

        r.ptr = str - 1;
        r.limit = r.ptr + size;
        w.limit = buf + sizeof(buf) - 1;
        do {
            /* One picky compiler complains if we initialize to buf - 1. */
            w.ptr = buf;  w.ptr--;
            status = (*templat->process) (st, &r, &w, true);
            stream_write(s, buf, (uint) (w.ptr + 1 - buf));
        }
        while (status == 1);
    }
}
static void
pdf_store_default_Producer(char *buf)
{
    if ((gs_revision % 100) == 0)
        gs_sprintf(buf, "(%s %1.1f)", gs_product, gs_revision / 100.0);
    else
        gs_sprintf(buf, "(%s %1.2f)", gs_product, gs_revision / 100.0);
}


int
pdf_image_close(gx_device * pdev)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
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
        stream_puts(pdf_dev->strm, "1 0 obj\n<<\n/Pages 2 0 R\n/Metadata 3 0 R\n/Type /Catalog\n>>\nendobj\n");

        pdf_dev->PagesOffset = stell(pdf_dev->strm);
        pprintd1(pdf_dev->strm, "2 0 obj\n<<\n/Count %d\n", pdf_dev->NumPages);
        stream_puts(pdf_dev->strm, "/Kids [");

        while(page){
            pprintd1(pdf_dev->strm, "%d 0 R ", page->PageDictObjectNumber);
            page = page->next;
        }

        stream_puts(pdf_dev->strm, "]\n/Type /Pages\n>>\nendobj\n");

        time(&t);
        tms = *gmtime(&t);
        tms.tm_isdst = -1;
        timeoffset = (int)difftime(t, mktime(&tms)); /* tz+dst in seconds */
        timesign = (timeoffset == 0 ? 'Z' : timeoffset < 0 ? '-' : '+');
        timeoffset = any_abs(timeoffset) / 60;
        tms = *localtime(&t);

        gs_sprintf(CreationDate, "(D:%04d%02d%02d%02d%02d%02d%c%02d\'%02d\')",
            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
            tms.tm_hour, tms.tm_min, tms.tm_sec,
            timesign, timeoffset / 60, timeoffset % 60);

        pdf_dev->xrefOffset = stell(pdf_dev->strm);
        pprintd1(pdf_dev->strm, "xref\n0 %d\n0000000000 65536 f \n", (pdf_dev->NumPages * 4) + 6);
        write_xref_entry(pdf_dev->strm, pdf_dev->RootOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->PagesOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->MetadataOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->MetadataLengthOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->InfoOffset);

        page = pdf_dev->Pages;
        while(page){
            write_xref_entry(pdf_dev->strm, page->ImageOffset);
            write_xref_entry(pdf_dev->strm, page->LengthOffset);
            write_xref_entry(pdf_dev->strm, page->PageStreamOffset);
            write_xref_entry(pdf_dev->strm, page->PageDictOffset);
            page = page->next;
        }
        pprintd1(pdf_dev->strm, "trailer\n<<\n/Size %d\n/Info 5 0 R\n/Root 1 0 R\n/ID [", (pdf_dev->NumPages * 4) + 6);
        pdf_compute_fileID(pdf_dev, fileID, CreationDate, Title, Producer);
        write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
        write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
        pprintd1(pdf_dev->strm, "]\n>>\nstartxref\n%d\n%%%%EOF\n", pdf_dev->xrefOffset);
    }

    if (pdf_dev->icclink != NULL)
    {
        pdf_dev->icclink->procs.free_link(pdf_dev->icclink);
        gsicc_free_link_dev(pdev->memory, pdf_dev->icclink);
        pdf_dev->icclink = NULL;
    }
    if (pdf_dev->strm) {
        sclose(pdf_dev->strm);
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
    }
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

static int
pdf_image_get_params(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_get_some_params(dev, plist, 0);
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
        {
            ecode = code;
            param_signal_error(plist, param_name, ecode);
        }
    }
    if (ecode < 0)
        return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        return code;

    return code;
}

static int
pdf_image_put_params(gx_device * dev, gs_param_list * plist)
{
    return pdf_image_put_some_params(dev, plist, 0);
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

/* ------ The PCLm device ------ */

static dev_proc_print_page(PCLm_print_page);

static const gx_device_procs PCLm_procs =
prn_color_params_procs(PCLm_open,
                       gdev_prn_output_page_seekable,
                       PCLm_close,
                       gx_default_rgb_map_rgb_color,
                       gx_default_rgb_map_color_rgb,
                       pdf_image_get_params_downscale,
                       pdf_image_put_params_downscale);

const gx_device_pdf_image gs_PCLm_device = {
    prn_device_body(gx_device_pdf_image,
                    PCLm_procs,
                    "PCLm",
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

/* Open a temporary file, with or without a stream. */
static int
PCLm_open_temp_file(gx_device_pdf_image *pdev, PCLm_temp_file_t *ptf)
{
    char fmode[4];

    if (strlen(gp_fmode_binary_suffix) > 2)
        return_error(gs_error_invalidfileaccess);

    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    ptf->file =	gp_open_scratch_file_64(pdev->memory,
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
    FILE *file = ptf->file;

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
        err = ferror(file) | fclose(file);
        unlink(ptf->file_name);
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
    gx_device_pdf_image *ppdev = (gx_device_pdf_image *)pdev;
    int code;
    bool update_procs = false;

    code = install_internal_subclass_devices((gx_device **)&pdev, &update_procs);
    if (code < 0)
        return code;
    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;

    ppdev->file = NULL;
    ppdev->Pages = NULL;
    ppdev->NumPages = 0;
    ppdev->RootOffset = 0;
    ppdev->PagesOffset = 0;
    ppdev->MetadataOffset = 0;
    ppdev->MetadataLengthOffset = 0;
    ppdev->InfoOffset = 0;
    ppdev->xrefOffset = 0;
    ppdev->NextObject = 0;
    gp_get_realtime(ppdev->uuid_time);
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
        code = gdev_prn_open_printer_seekable(pdev, 1, true);

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
                         FILE *file)
{
    gx_device_printer *const pdev = (gx_device_printer *)pdf_dev;
    cmm_dev_profile_t *profile_struct;
    gsicc_rendering_param_t rendering_params;
    int code;
    pdfimage_page *page;

    page = (pdfimage_page *)gs_alloc_bytes(pdf_dev->memory->non_gc_memory, sizeof(pdfimage_page), "pdfimage create new page");
    if (page == NULL)
        return_error(gs_error_VMerror);

    memset(page, 0x00, sizeof(pdfimage_page));

    if (gdev_prn_file_is_new(pdev)) {
        /* Set up the icc link settings at this time */
        code = dev_proc(pdev, get_profile)((gx_device *)pdev, &profile_struct);
        if (code < 0)
            return_error(gs_error_undefined);
        if (profile_struct->postren_profile != NULL) {
            rendering_params.black_point_comp = gsBLACKPTCOMP_ON;
            rendering_params.graphics_type_tag = GS_UNKNOWN_TAG;
            rendering_params.override_icc = false;
            rendering_params.preserve_black = gsBLACKPRESERVE_OFF;
            rendering_params.rendering_intent = gsRELATIVECOLORIMETRIC;
            rendering_params.cmm = gsCMM_DEFAULT;
            if (profile_struct->oi_profile != NULL) {
                pdf_dev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->oi_profile, profile_struct->postren_profile,
                    &rendering_params);
            } else if (profile_struct->link_profile != NULL) {
                pdf_dev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->link_profile, profile_struct->postren_profile,
                    &rendering_params);
            } else {
                pdf_dev->icclink = gsicc_alloc_link_dev(pdev->memory,
                    profile_struct->device_profile[0], profile_struct->postren_profile,
                    &rendering_params);
            }
            /* If it is identity, release it now and set link to NULL */
            if (pdf_dev->icclink->is_identity) {
                pdf_dev->icclink->procs.free_link(pdf_dev->icclink);
                gsicc_free_link_dev(pdev->memory, pdf_dev->icclink);
                pdf_dev->icclink = NULL;
            }
        }

        /* Set up the stream and insert the file header */
        pdf_dev->strm = s_alloc(pdf_dev->memory->non_gc_memory, "pdfimage_open_temp_stream(strm)");
        if (pdf_dev->strm == 0)
            return_error(gs_error_VMerror);
        pdf_dev->strm_buf = gs_alloc_bytes(pdf_dev->memory->non_gc_memory, 512,
                                       "pdfimage_open_temp_stream(strm_buf)");
        if (pdf_dev->strm_buf == 0) {
            gs_free_object(pdf_dev->memory->non_gc_memory, pdf_dev->strm,
                           "pdfimage_open_temp_stream(strm)");
            pdf_dev->strm = 0;
            return_error(gs_error_VMerror);
        }
        swrite_file(pdf_dev->strm, pdf_dev->file, pdf_dev->strm_buf, 512);

        stream_puts(pdf_dev->strm, "%PDF-1.3\n");
        stream_puts(pdf_dev->strm, "%PCLm 1.0\n");
        pdf_dev->Pages = page;
        pdf_dev->NextObject = 3;
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
PCLm_downscale_and_print_page(gx_device_printer *dev, int factor,
                              int mfs, int bpc, int num_comps,
                              int trap_w, int trap_h, const int *trap_order,
                              int ets)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int code = 0;
    uint Read;
    byte *data = NULL;
    int size = gdev_mem_bytes_per_scan_line((gx_device *)dev);
    int max_size =size;
    int row, NumStrips;
    double StripDecrement;
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
        code = gx_downscaler_init_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
            factor, mfs, NULL, /*&fax_adjusted_width*/ 0, /*aw*/ ets);
    } else {
        code = gx_downscaler_init_cm_ets(&ds, (gx_device *)dev, 8, bpc, num_comps,
            factor, mfs, NULL, /*&fax_adjusted_width*/ 0, /*aw*/ pdf_image_chunky_post_cm, pdf_dev->icclink,
            pdf_dev->icclink->num_output, ets);
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
    gs_sprintf(Buffer, "/MediaBox [0 0 %f %f]\n", ((double)pdf_dev->width / pdf_dev->HWResolution[0]) * 72, ((double)pdf_dev->height / pdf_dev->HWResolution[1]) * 72);
    stream_puts(pdf_dev->strm, Buffer);
    stream_puts(pdf_dev->strm, "/Resources <<\n/XObject <<\n");

    if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
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
            gs_sprintf(Buffer, "%f 0 0 %f 0 0 cm\n/Im%d Do Q\n", (width / (pdf_dev->HWResolution[0] / 72)) * factor, adjusted, row);
        } else
            gs_sprintf(Buffer, "%f 0 0 %f 0 %f cm\n/Im%d Do Q\n", (width / (pdf_dev->HWResolution[0] / 72)) * factor, StripDecrement, ((height / (pdf_dev->HWResolution[1] / 72)) * factor) - (StripDecrement * (row + 1)), row);
        stream_puts(pdf_dev->temp_stream.strm, Buffer);
        pprintd2(pdf_dev->strm, "/Im%d %d 0 R\n", row, page->ImageObjectNumber + (row * 2));
    }
    sflush(pdf_dev->temp_stream.strm);
    stream_pos = gp_ftell_64(pdf_dev->temp_stream.file);
    stream_puts(pdf_dev->strm, ">>\n>>\n>>\nendobj\n");

    page->PageStreamOffset = stell(pdf_dev->strm);
    write_xref_entry(pdf_dev->xref_stream.strm, page->PageStreamOffset);
    pprintd1(pdf_dev->strm, "%d 0 obj\n", page->PageStreamObjectNumber);
    pprintd1(pdf_dev->strm, "<<\n/Length %d\n>>\nstream\n", stream_pos);

    if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
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

    if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
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
            stream_pos = gp_ftell_64(pdf_dev->temp_stream.file);

            page->ImageOffset = stell(pdf_dev->strm);
            write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
            pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
            pprintd1(pdf_dev->strm, "<<\n/Length %d\n", stream_pos);
            stream_puts(pdf_dev->strm, "/Subtype /Image\n");
            pprintd1(pdf_dev->strm, "/Width %d\n", width);
            pprintd1(pdf_dev->strm, "/Height %d\n", Read);
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
            if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
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

            if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
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
        uint R = 0;

        pdf_dev->StripHeight = Read;
        if (pdf_dev->temp_stream.save != pdf_dev->temp_stream.strm)
            s_close_filters(&pdf_dev->temp_stream.strm, pdf_dev->temp_stream.save);
        sflush(pdf_dev->temp_stream.strm);
        stream_pos = gp_ftell_64(pdf_dev->temp_stream.file);

        page->ImageOffset = stell(pdf_dev->strm);
        write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
        pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
        pprintd1(pdf_dev->strm, "<<\n/Length %d\n", stream_pos);
        stream_puts(pdf_dev->strm, "/Subtype /Image\n");
        pprintd1(pdf_dev->strm, "/Width %d\n", width);
        pprintd1(pdf_dev->strm, "/Height %d\n", Read);
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
        if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
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

        if (gp_fseek_64(pdf_dev->temp_stream.file, 0, SEEK_SET) != 0) {
            gs_free_object(dev->memory, data, "pdf_image_print_page(data)");
            gx_downscaler_fin(&ds);
            return_error(gs_error_ioerror);
        }
        page->ImageOffset = stell(pdf_dev->strm);
        write_xref_entry(pdf_dev->xref_stream.strm, page->ImageOffset);
        pprintd1(pdf_dev->strm, "%d 0 obj\n", page->ImageObjectNumber++);
        stream_puts(pdf_dev->strm, "<</Length 14>>\nstream\nq /image Do Q\nendstream\nendobj\n");
    }
    pdf_dev->NextObject = page->ImageObjectNumber;

    gx_downscaler_fin(&ds);
    gs_free_object(dev->memory, data, "pdf_image_print_page(data)");

    pdf_dev->NumPages++;
    return code;
}

static int
PCLm_print_page(gx_device_printer * pdev, FILE * file)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    int code;

    code = gdev_PCLm_begin_page(pdf_dev, file);
    if (code < 0)
        return code;

    code = PCLm_downscale_and_print_page(pdev,
                                         pdf_dev->downscale.downscale_factor,
                                         pdf_dev->downscale.min_feature_size,
                                         8, pdf_dev->color_info.num_components,
                                         0, 0, NULL,
                                         pdf_dev->downscale.ets);
    if (code < 0)
        return code;

    return 0;
}

int
PCLm_close(gx_device * pdev)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)pdev;
    pdfimage_page *page = pdf_dev->Pages;
    int code, code1;
    struct tm tms;
    time_t t;
    int timeoffset;
    char timesign;
    char Buffer[1024], CreationDate[26], Title[] = "Untitled", Producer[256];
    gs_offset_t streamsize, R = 0;

    if (pdf_dev->strm != NULL) {
        byte fileID[16];

        pdf_store_default_Producer(Producer);

        pdf_dev->RootOffset = stell(pdf_dev->strm);
        stream_puts(pdf_dev->strm, "1 0 obj\n<<\n/Pages 2 0 R\n/Type /Catalog\n>>\nendobj\n");

        pdf_dev->PagesOffset = stell(pdf_dev->strm);
        pprintd1(pdf_dev->strm, "2 0 obj\n<<\n/Count %d\n", pdf_dev->NumPages);
        stream_puts(pdf_dev->strm, "/Kids [");

        while(page){
            pprintd1(pdf_dev->strm, "%d 0 R ", page->PageDictObjectNumber);
            page = page->next;
        }

        stream_puts(pdf_dev->strm, "]\n/Type /Pages\n>>\nendobj\n");

        time(&t);
        tms = *gmtime(&t);
        tms.tm_isdst = -1;
        timeoffset = (int)difftime(t, mktime(&tms)); /* tz+dst in seconds */
        timesign = (timeoffset == 0 ? 'Z' : timeoffset < 0 ? '-' : '+');
        timeoffset = any_abs(timeoffset) / 60;
        tms = *localtime(&t);

        gs_sprintf(CreationDate, "(D:%04d%02d%02d%02d%02d%02d%c%02d\'%02d\')",
            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
            tms.tm_hour, tms.tm_min, tms.tm_sec,
            timesign, timeoffset / 60, timeoffset % 60);

        pdf_dev->xrefOffset = stell(pdf_dev->strm);
        pprintd1(pdf_dev->strm, "xref\n0 %d\n0000000000 65536 f \n", pdf_dev->NextObject);
        write_xref_entry(pdf_dev->strm, pdf_dev->RootOffset);
        write_xref_entry(pdf_dev->strm, pdf_dev->PagesOffset);

        sflush(pdf_dev->xref_stream.strm);
        streamsize = gp_ftell_64(pdf_dev->xref_stream.file);
        if (gp_fseek_64(pdf_dev->xref_stream.file, 0, SEEK_SET) != 0)
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

        pprintd1(pdf_dev->strm, "trailer\n<<\n/Size %d\n/Root 1 0 R\n/ID [", pdf_dev->NextObject);
        pdf_compute_fileID(pdf_dev, fileID, CreationDate, Title, Producer);
        write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
        write_fileID(pdf_dev->strm, (const byte *)&fileID, 16);
        pprintd1(pdf_dev->strm, "]\n>>\nstartxref\n%d\n%%%%EOF\n", pdf_dev->xrefOffset);
    }

    if (pdf_dev->icclink != NULL)
    {
        pdf_dev->icclink->procs.free_link(pdf_dev->icclink);
        gsicc_free_link_dev(pdev->memory, pdf_dev->icclink);
        pdf_dev->icclink = NULL;
    }
    if (pdf_dev->strm) {
        sclose(pdf_dev->strm);
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
    }
    code = PCLm_close_temp_file(pdf_dev, &pdf_dev->xref_stream, 0);
    code1 = PCLm_close_temp_file(pdf_dev, &pdf_dev->temp_stream, 0);
    if (code == 0)
        code = code1;
    code1 = gdev_prn_close(pdev);
    if (code == 0)
        code = code1;
    return code;
}
