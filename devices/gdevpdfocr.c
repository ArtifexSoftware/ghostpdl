/* Copyright (C) 2001-2024 Artifex Software, Inc.
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
#include "tessocr.h"

static dev_proc_initialize_device(pdf_ocr_initialize_device);
static dev_proc_open_device(pdf_ocr_open);
static dev_proc_close_device(pdf_ocr_close);

static int
pdfocr_put_some_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int ecode = 0;
    int code;
    gs_param_string langstr;
    const char *param_name;
    size_t len;
    int engine;

    switch (code = param_read_string(plist, (param_name = "OCRLanguage"), &langstr)) {
        case 0:
                if (gs_is_path_control_active(pdf_dev->memory)
                && (strlen(pdf_dev->ocr.language) != langstr.size || memcmp(pdf_dev->ocr.language, langstr.data, langstr.size) != 0)) {
                return_error(gs_error_invalidaccess);
            }
            else {
                len = langstr.size;
                if (len >= sizeof(pdf_dev->ocr.language))
                    len = sizeof(pdf_dev->ocr.language)-1;
                memcpy(pdf_dev->ocr.language, langstr.data, len);
                pdf_dev->ocr.language[len] = 0;
            }
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }

    switch (code = param_read_int(plist, (param_name = "OCREngine"), &engine)) {
        case 0:
            pdf_dev->ocr.engine = engine;
            break;
        case 1:
            break;
        default:
            ecode = code;
            param_signal_error(plist, param_name, ecode);
    }

    return code;
}

static int
pdfocr_put_params_downscale_cmyk(gx_device * dev, gs_param_list * plist)
{
    int code = pdfocr_put_some_params(dev, plist);
    if (code < 0)
        return code;
    return pdf_image_put_params_downscale_cmyk(dev, plist);
}

static int
pdfocr_put_params_downscale(gx_device * dev, gs_param_list * plist)
{
    int code = pdfocr_put_some_params(dev, plist);
    if (code < 0)
        return code;
    return pdf_image_put_params_downscale(dev, plist);
}

static int
pdfocr_get_some_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_pdf_image *const pdf_dev = (gx_device_pdf_image *)dev;
    int code = 0;
    int ecode = 0;
    gs_param_string langstr;

    if (pdf_dev->ocr.language[0]) {
        langstr.data = (const byte *)pdf_dev->ocr.language;
        langstr.size = strlen(pdf_dev->ocr.language);
        langstr.persistent = false;
    } else {
        langstr.data = (const byte *)"eng";
        langstr.size = 3;
        langstr.persistent = false;
    }
    if ((code = param_write_string(plist, "OCRLanguage", &langstr)) < 0)
        ecode = code;

    if ((code = param_write_int(plist, "OCREngine", &pdf_dev->ocr.engine)) < 0)
        ecode = code;

    return ecode;
}

static int
pdfocr_get_params_downscale_cmyk(gx_device * dev, gs_param_list * plist)
{
    int code = pdfocr_get_some_params(dev, plist);
    if (code < 0)
        return code;

    return pdf_image_get_params_downscale_cmyk(dev, plist);
}

static int
pdfocr_get_params_downscale(gx_device * dev, gs_param_list * plist)
{
    int code = pdfocr_get_some_params(dev, plist);
    if (code < 0)
        return code;

    return pdf_image_get_params_downscale(dev, plist);
}

/* ------ The pdfocr8 device ------ */
static void
pdfocr8_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_gray(dev);

    set_dev_proc(dev, initialize_device, pdf_ocr_initialize_device);
    set_dev_proc(dev, initialize_device, pdf_ocr_initialize_device);
    set_dev_proc(dev, open_device, pdf_ocr_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, pdf_ocr_close);
    set_dev_proc(dev, get_params, pdfocr_get_params_downscale);
    set_dev_proc(dev, put_params, pdfocr_put_params_downscale);
    set_dev_proc(dev, encode_color, gx_default_8bit_map_gray_color);
    set_dev_proc(dev, decode_color, gx_default_8bit_map_color_gray);
}

const gx_device_pdf_image gs_pdfocr8_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfocr8_initialize_device_procs,
                    "pdfocr8",
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

/* ------ The pdfocr24 device ------ */
static void
pdfocr24_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_rgb(dev);

    set_dev_proc(dev, initialize_device, pdf_ocr_initialize_device);
    set_dev_proc(dev, open_device, pdf_ocr_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, pdf_ocr_close);
    set_dev_proc(dev, get_params, pdfocr_get_params_downscale);
    set_dev_proc(dev, put_params, pdfocr_put_params_downscale);
}

const gx_device_pdf_image gs_pdfocr24_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfocr24_initialize_device_procs,
                    "pdfocr24",
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

/* ------ The pdfocr32 device ------ */
static void
pdfocr32_initialize_device_procs(gx_device *dev)
{
    gdev_prn_initialize_device_procs_cmyk8(dev);

    set_dev_proc(dev, initialize_device, pdf_ocr_initialize_device);
    set_dev_proc(dev, open_device, pdf_ocr_open);
    set_dev_proc(dev, output_page, gdev_prn_output_page_seekable);
    set_dev_proc(dev, close_device, pdf_ocr_close);
    set_dev_proc(dev, get_params, pdfocr_get_params_downscale_cmyk);
    set_dev_proc(dev, put_params, pdfocr_put_params_downscale_cmyk);
    set_dev_proc(dev, decode_color, cmyk_8bit_map_color_cmyk);
    set_dev_proc(dev, encode_color, cmyk_8bit_map_cmyk_color);
}

const gx_device_pdf_image gs_pdfocr32_device = {
    prn_device_body(gx_device_pdf_image,
                    pdfocr32_initialize_device_procs,
                    "pdfocr32",
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

/* These strings have object numbers built into them which are only
 * correct for a specific value of PDFIMG_STATIC_OBJS (currently 4)
 * If we add more static objects (eg Metadata) then we need to update
 * the values in the strings below.
 */

/* Funky font */
static const char funky_font[] =
"4 0 obj\n<</BaseFont/GlyphLessFont/DescendantFonts[5 0 R]"
"/Encoding/Identity-H/Subtype/Type0/ToUnicode 7 0 R/Type /Font"
">>\nendobj\n";

static const char funky_font2[] =
"5 0 obj\n<</BaseFont/GlyphLessFont"
"/CIDToGIDMap 6 0 R\n/CIDSystemInfo<<\n"
"/Ordering (Identity)/Registry (Adobe)/Supplement 0>>"
"/FontDescriptor 8 0 R/Subtype /CIDFontType2/Type/Font"
"/DW 500>>\nendobj\n";

static const char funky_font3[] =
"6 0 obj\n<</Length 210/Filter/FlateDecode"
">>stream\n";

static const char funky_font3a[] = {
0x78, 0x9c, 0xec, 0xc2, 0x01, 0x09, 0x00, 0x00,
0x00, 0x02, 0xa0, 0xfa, 0x7f, 0xba, 0x21, 0x89,
0xa6, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x80, 0x7b, 0x03, 0x00, 0x00, 0xff, 0xff, 0xec,
0xc2, 0x01, 0x0d, 0x00, 0x00, 0x00, 0xc2, 0x20,
0xdf, 0xbf, 0xb4, 0x45, 0x18, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x00, 0x00,
0x00, 0xff, 0xff, 0xec, 0xc2, 0x01, 0x0d, 0x00,
0x00, 0x00, 0xc2, 0x20, 0xdf, 0xbf, 0xb4, 0x45,
0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0xeb, 0x00, 0x00, 0x00, 0xff, 0xff, 0xed,
0xc2, 0x01, 0x0d, 0x00, 0x00, 0x00, 0xc2, 0x20,
0xdf, 0xbf, 0xb4, 0x45, 0x18, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x00, 0xff,
0x00, 0x10
};

static const char funky_font3b[] =
"endstream\nendobj\n";

static const char funky_font4[] =
"7 0 obj\n<</Length 353>>\nstream\n"
"/CIDInit /ProcSet findresource begin\n"
"12 dict begin\n"
"begincmap\n"
"/CIDSystemInfo\n"
"<<\n"
"  /Registry (Adobe)\n"
"  /Ordering (UCS)\n"
"  /Supplement 0\n"
">> def\n"
"/CMapName /Adobe-Identify-UCS def\n"
"/CMapType 2 def\n"
"1 begincodespacerange\n"
"<0000> <FFFF>\n"
"endcodespacerange\n"
"1 beginbfrange\n"
"<0000> <FFFF> <0000>\n"
"endbfrange\n"
"endcmap\n"
"CMapName currentdict /CMap defineresource pop\n"
"end\n"
"end\n"
"endstream\n"
"endobj\n";

static const char funky_font5[] =
"8 0 obj\n"
"<</Ascent 1000/CapHeight 1000/Descent -1/Flags 5"
"/FontBBox[0 0 500 1000]/FontFile2 9 0 R/FontName/GlyphLessFont"
"/ItalicAngle 0/StemV 80/Type/FontDescriptor>>\nendobj\n";

static const char funky_font6[] =
"9 0 obj\n<</Length 572/Length1 572>>\nstream\n";

static const char funky_font6a[] =
{
0x00, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x80,
0x00, 0x03, 0x00, 0x20, 0x4f, 0x53, 0x2f, 0x32,
0x56, 0xde, 0xc8, 0x94, 0x00, 0x00, 0x01, 0x28,
0x00, 0x00, 0x00, 0x60, 0x63, 0x6d, 0x61, 0x70,
0x00, 0x0a, 0x00, 0x34, 0x00, 0x00, 0x01, 0x90,
0x00, 0x00, 0x00, 0x1e, 0x67, 0x6c, 0x79, 0x66,
0x15, 0x22, 0x41, 0x24, 0x00, 0x00, 0x01, 0xb8,
0x00, 0x00, 0x00, 0x18, 0x68, 0x65, 0x61, 0x64,
0x0b, 0x78, 0xf1, 0x65, 0x00, 0x00, 0x00, 0xac,
0x00, 0x00, 0x00, 0x36, 0x68, 0x68, 0x65, 0x61,
0x0c, 0x02, 0x04, 0x02, 0x00, 0x00, 0x00, 0xe4,
0x00, 0x00, 0x00, 0x24, 0x68, 0x6d, 0x74, 0x78,
0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x88,
0x00, 0x00, 0x00, 0x08, 0x6c, 0x6f, 0x63, 0x61,
0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x01, 0xb0,
0x00, 0x00, 0x00, 0x06, 0x6d, 0x61, 0x78, 0x70,
0x00, 0x04, 0x00, 0x05, 0x00, 0x00, 0x01, 0x08,
0x00, 0x00, 0x00, 0x20, 0x6e, 0x61, 0x6d, 0x65,
0xf2, 0xeb, 0x16, 0xda, 0x00, 0x00, 0x01, 0xd0,
0x00, 0x00, 0x00, 0x4b, 0x70, 0x6f, 0x73, 0x74,
0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x1c,
0x00, 0x00, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00,
0x00, 0x01, 0x00, 0x00, 0xb0, 0x94, 0x71, 0x10,
0x5f, 0x0f, 0x3c, 0xf5, 0x04, 0x07, 0x08, 0x00,
0x00, 0x00, 0x00, 0x00, 0xcf, 0x9a, 0xfc, 0x6e,
0x00, 0x00, 0x00, 0x00, 0xd4, 0xc3, 0xa7, 0xf2,
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x08, 0x00,
0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
0x08, 0x00, 0xff, 0xff, 0x00, 0x00, 0x04, 0x00,
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04,
0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x03, 0x00, 0x00, 0x01, 0x90, 0x00, 0x05,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x05, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x47, 0x4f, 0x4f, 0x47, 0x00, 0x40,
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff,
0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x80, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x14, 0x00, 0x03, 0x00, 0x00,
0x00, 0x00, 0x00, 0x14, 0x00, 0x06, 0x00, 0x0a,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00,
0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x31, 0x21,
0x11, 0x21, 0x04, 0x00, 0xfc, 0x00, 0x08, 0x00,
0x00, 0x00, 0x00, 0x03, 0x00, 0x2a, 0x00, 0x00,
0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x16,
0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
0x00, 0x05, 0x00, 0x0b, 0x00, 0x16, 0x00, 0x03,
0x00, 0x01, 0x04, 0x09, 0x00, 0x05, 0x00, 0x16,
0x00, 0x00, 0x00, 0x56, 0x00, 0x65, 0x00, 0x72,
0x00, 0x73, 0x00, 0x69, 0x00, 0x6f, 0x00, 0x6e,
0x00, 0x20, 0x00, 0x31, 0x00, 0x2e, 0x00, 0x30,
0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x20,
0x31, 0x2e, 0x30, 0x00, 0x00, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00
};

static const char funky_font6b[] =
"endstream\nendobj\n";

static int
ocr_file_init(gx_device_pdf_image *dev)
{
    const char *language = dev->ocr.language;
    if (language == NULL || language[0] == 0)
        language = "eng";

    dev->ocr.file_object_offset[dev->ocr.file_objects++] = stell(dev->strm);
    stream_write(dev->strm, funky_font,  sizeof(funky_font)-1);
    dev->ocr.file_object_offset[dev->ocr.file_objects++] = stell(dev->strm);
    stream_write(dev->strm, funky_font2, sizeof(funky_font2)-1);
    dev->ocr.file_object_offset[dev->ocr.file_objects++] = stell(dev->strm);
    stream_write(dev->strm, funky_font3, sizeof(funky_font3)-1);
    stream_write(dev->strm, funky_font3a, sizeof(funky_font3a));
    stream_write(dev->strm, funky_font3b, sizeof(funky_font3b)-1);
    dev->ocr.file_object_offset[dev->ocr.file_objects++] = stell(dev->strm);
    stream_write(dev->strm, funky_font4, sizeof(funky_font4)-1);
    dev->ocr.file_object_offset[dev->ocr.file_objects++] = stell(dev->strm);
    stream_write(dev->strm, funky_font5, sizeof(funky_font5)-1);
    dev->ocr.file_object_offset[dev->ocr.file_objects++] = stell(dev->strm);
    stream_write(dev->strm, funky_font6, sizeof(funky_font6)-1);
    stream_write(dev->strm, funky_font6a, sizeof(funky_font6a));
    stream_write(dev->strm, funky_font6b, sizeof(funky_font6b)-1);

    return ocr_init_api(dev->memory->non_gc_memory, language, dev->ocr.engine, &dev->ocr.state);
}

static void
ocr_line8(gx_device_pdf_image *dev, void *row)
{
    int w = dev->ocr.w;
    int raster = (w+3)&~3;
    char *in = (char *)row;
    char *out = ((char *)dev->ocr.data) + raster * dev->ocr.y++;
    int i;

#if ARCH_IS_BIG_ENDIAN
    memcpy(out, in, dev->ocr.w);
#else
    for (i = 0; i < w; i++)
        out[i^3] = in[i];
#endif
}

static void
ocr_line24(gx_device_pdf_image *dev, void *row)
{
    int w = dev->ocr.w;
    int raster = (w+3)&~3;
    char *in = (char *)row;
    char *out = ((char *)dev->ocr.data) + raster * dev->ocr.y++;
    int i;

#if ARCH_IS_BIG_ENDIAN
    for (i = 0; i < w; i++) {
        int v = *in++;
        v += 2* *in++;
        v +=    *in++;
        out[i] = v>>2;
    }
#else
    for (i = 0; i < w; i++) {
        int v = *in++;
        v += 2* *in++;
        v +=    *in++;
        out[i^3] = v>>2;
    }
#endif
}

static void
ocr_line32(gx_device_pdf_image *dev, void *row)
{
    int w = dev->ocr.w;
    int raster = (w+3)&~3;
    char *in = (char *)row;
    char *out = ((char *)dev->ocr.data) + raster * dev->ocr.y++;
    int i;

#if ARCH_IS_BIG_ENDIAN
    for (i = 0; i < w; i++) {
        int v = 255 - *in++;
        v -= *in++;
        v -= *in++;
        v -= *in++;
        if (v < 0) v = 0;
        out[i] = v;
    }
#else
    for (i = 0; i < w; i++) {
        int v = 255 - *in++;
        v -= *in++;
        v -= *in++;
        v -= *in++;
        if (v < 0) v = 0;
        out[i^3] = v;
    }
#endif
}

static int
ocr_begin_page(gx_device_pdf_image *dev, int w, int h, int bpp)
{
    int raster = (w+3)&~3;

    dev->ocr.data = gs_alloc_bytes(dev->memory, raster * h, "ocr_begin_page");
    if (dev->ocr.data == NULL)
        return_error(gs_error_VMerror);
    dev->ocr.w = w;
    dev->ocr.h = h;
    dev->ocr.y = 0;

    if (bpp == 32)
        dev->ocr.line = ocr_line32;
    else if (bpp == 24)
        dev->ocr.line = ocr_line24;
    else
        dev->ocr.line = ocr_line8;

    return 0;
}

static void
flush_word(gx_device_pdf_image *dev)
{
    char buffer[1024];
    float size, scale;
    float *bbox = dev->ocr.wordbox;
    int i, len;

    len = dev->ocr.word_len;
    if (len == 0)
        return;

    size = bbox[3]-bbox[1];
    if (dev->ocr.cur_size != size) {
        gs_snprintf(buffer, sizeof(buffer), "/Ft0 %.3f Tf", size);
        stream_puts(dev->strm, buffer);
        dev->ocr.cur_size = size;
    }
    scale = (bbox[2]-bbox[0]) / size / len * 200;
    if (dev->ocr.cur_scale != scale) {
        gs_snprintf(buffer, sizeof(buffer), " %.3f Tz", scale);
        stream_puts(dev->strm, buffer);
        dev->ocr.cur_scale = scale;
    }
    gs_snprintf(buffer, sizeof(buffer), " 1 0 0 1 %.3f %.3f Tm[<", bbox[0], bbox[1]);
    stream_puts(dev->strm, buffer);
    for (i = 0; i < len; i++) {
        gs_snprintf(buffer, sizeof(buffer), "%04x", dev->ocr.word_chars[i]);
        stream_puts(dev->strm, buffer);
    }
    stream_puts(dev->strm, ">]TJ\n");

    dev->ocr.word_len = 0;
}

static int
ocr_callback(void *arg, const char *rune_,
             const int *line_bbox, const int *word_bbox,
             const int *char_bbox, int pointsize)
{
    gx_device_pdf_image *ppdev = (gx_device_pdf_image *)arg;
    int unicode;
    const unsigned char *rune = (const unsigned char *)rune_;
    float bbox[4];
    int factor = ppdev->downscale.downscale_factor;
    float scale = 72000000.0f / gx_downscaler_scale(1000000, factor);

    if (rune[0] >= 0xF8)
        return 0; /* Illegal */
    if (rune[0] < 0x80)
        unicode = rune[0];
    else {
        unicode = rune[1] & 0x7f;
        if (rune[0] < 0xd0)
            unicode |= ((rune[0] & 0x1f) << 6);
        else {
            unicode = (unicode<<6) | (rune[2] & 0x7f);
            if (rune[0] < 0xf0)
                unicode |= ((rune[0] & 0x0f) << 12);
            else
                unicode |= ((rune[0] & 0x07) << 18) | (unicode<<6) | (rune[3] & 0x7f);
        }
    }

#if 0
    // First attempt; match char bboxes exactly. This is bad, as the
    // bboxes given back from tesseract are 'untrustworthy' to say the
    // least (they overlap one another in strange ways). Trying to
    // match those causes the font height to change repeatedly, and
    // gives output that's hard to identify words in.
    bbox[0] = char_bbox[0] * 72.0 / ppdev->ocr.xres;
    bbox[1] = (ppdev->ocr.h-1 - char_bbox[3]) * 72.0 / ppdev->ocr.yres;
    bbox[2] = char_bbox[2] * 72.0 / ppdev->ocr.xres;
    bbox[3] = (ppdev->ocr.h-1 - char_bbox[1]) * 72.0 / ppdev->ocr.yres;

    size = bbox[3]-bbox[1];
    if (ppdev->ocr.cur_size != size) {
        gs_snprintf(buffer, sizeof(buffer), "/Ft0 %f Tf ", size);
        stream_puts(ppdev->strm, buffer);
        ppdev->ocr.cur_size = size;
    }
    scale = (bbox[2]-bbox[0]) / size * 200;
    if (ppdev->ocr.cur_scale != scale) {
        gs_snprintf(buffer, sizeof(buffer), " %f Tz ", scale);
        stream_puts(ppdev->strm, buffer);
        ppdev->ocr.cur_scale = scale;
    }
    gs_snprintf(buffer, sizeof(buffer), "1 0 0 1 %f %f Tm ", bbox[0], bbox[1]);
    stream_puts(ppdev->strm, buffer);
    gs_snprintf(buffer, sizeof(buffer), "<%04x>Tj\n", unicode);
    stream_puts(ppdev->strm, buffer);
#else
    bbox[0] = word_bbox[0] * scale / ppdev->ocr.xres;
    bbox[1] = (ppdev->ocr.h-1 - line_bbox[3]) * scale / ppdev->ocr.yres;
    bbox[2] = word_bbox[2] * scale / ppdev->ocr.xres;
    bbox[3] = (ppdev->ocr.h-1 - line_bbox[1]) * scale / ppdev->ocr.yres;

    /* If the word bbox differs, flush the word. */
    if (bbox[0] != ppdev->ocr.wordbox[0] ||
        bbox[1] != ppdev->ocr.wordbox[1] ||
        bbox[2] != ppdev->ocr.wordbox[2] ||
        bbox[3] != ppdev->ocr.wordbox[3]) {
        flush_word(ppdev);
        ppdev->ocr.wordbox[0] = bbox[0];
        ppdev->ocr.wordbox[1] = bbox[1];
        ppdev->ocr.wordbox[2] = bbox[2];
        ppdev->ocr.wordbox[3] = bbox[3];
    }

    /* Add the char to the current word. */
    if (ppdev->ocr.word_len == ppdev->ocr.word_max) {
        int *newblock;
        int newmax = ppdev->ocr.word_max * 2;
        if (newmax == 0)
            newmax = 16;
        newblock = (int *)gs_alloc_bytes(ppdev->memory, sizeof(int)*newmax,
                                         "ocr_callback(word)");
        if (newblock == NULL)
            return_error(gs_error_VMerror);
        if (ppdev->ocr.word_len > 0)
            memcpy(newblock, ppdev->ocr.word_chars,
                   sizeof(int) * ppdev->ocr.word_len);
        gs_free_object(ppdev->memory, ppdev->ocr.word_chars,
                       "ocr_callback(word)");
        ppdev->ocr.word_chars = newblock;
        ppdev->ocr.word_max = newmax;
    }
    ppdev->ocr.word_chars[ppdev->ocr.word_len++] = unicode;
#endif

    return 0;
}

static int
ocr_end_page(gx_device_pdf_image *dev)
{
    stream_puts(dev->strm, "\nBT 3 Tr\n");
    dev->ocr.cur_x = 0;
    dev->ocr.cur_y = 0;
    dev->ocr.cur_size = -1;
    dev->ocr.cur_scale = 0;
    dev->ocr.wordbox[0] = 0;
    dev->ocr.wordbox[1] = 0;
    dev->ocr.wordbox[2] = -1;
    dev->ocr.wordbox[3] = -1;
    dev->ocr.word_len = 0;
    dev->ocr.word_max = 0;
    dev->ocr.word_chars = NULL;
    ocr_recognise(dev->ocr.state,
                  dev->ocr.w,
                  dev->ocr.h,
                  dev->ocr.data,
                  dev->ocr.xres,
                  dev->ocr.yres,
                  ocr_callback,
                  dev);
    if (dev->ocr.word_len)
        flush_word(dev);
    stream_puts(dev->strm, "\nET");

    gs_free_object(dev->memory, dev->ocr.word_chars,
                   "ocr_callback(word)");
    gs_free_object(dev->memory, dev->ocr.data, "ocr_end_page");
    dev->ocr.data = NULL;

    return 0;
}

static int
pdf_ocr_initialize_device(gx_device *dev)
{
    gx_device_pdf_image *ppdev = (gx_device_pdf_image *)dev;
    const char *default_ocr_lang = "eng";

    ppdev->ocr.language[0] = '\0';
    strcpy(ppdev->ocr.language, default_ocr_lang);
    return 0;
}

static int
pdf_ocr_open(gx_device *pdev)
{
    gx_device_pdf_image *ppdev;
    int code = pdf_image_open(pdev);

    if (code < 0)
        return code;

    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;
    ppdev = (gx_device_pdf_image *)pdev;

    ppdev->ocr.file_init  = ocr_file_init;
    ppdev->ocr.begin_page = ocr_begin_page;
    ppdev->ocr.end_page   = ocr_end_page;
    ppdev->ocr.xres = (int)pdev->HWResolution[0];
    ppdev->ocr.yres = (int)pdev->HWResolution[1];

    return 0;
}

static int
pdf_ocr_close(gx_device *pdev)
{
    gx_device_pdf_image *pdf_dev;
    int code;

    code = pdf_image_close(pdev);
    if (code < 0)
        return code;

    /* If we've been subclassed, find the terminal device */
    while(pdev->child)
        pdev = pdev->child;
    pdf_dev = (gx_device_pdf_image *)pdev;

    ocr_fin_api(pdf_dev->memory, pdf_dev->ocr.state);
    pdf_dev->ocr.state = NULL;

    return code;
}
