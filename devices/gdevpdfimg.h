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

#ifndef gdevpdfimg_h_INCLUDED
#define gdevpdfimg_h_INCLUDED

#include "gdevprn.h"
#include "gxdownscale.h"

/* This is the number of always defined objects; object 0 the head
 * of the free list, Object 1 the Root dictionary, Object 2
 * the Pages dictionary and object 3 the Info dictionary
 */
#define PDFIMG_STATIC_OBJS 4
#define OCR_MAX_FILE_OBJECTS 8
/* This is the number of objects per page: 1: the image object,
 * 2: the length object for the image object, 3: the page stream
 * object. 4: the page dict object, 5: the length object for
 * the page stream object. */
#define PDFIMG_OBJS_PER_PAGE 5

typedef struct pdfimage_page_s {
    int ImageObjectNumber;
    gs_offset_t ImageOffset;
    int LengthObjectNumber;
    gs_offset_t LengthOffset;
    int PageStreamObjectNumber;
    gs_offset_t PageStreamOffset;
    int PageDictObjectNumber;
    gs_offset_t PageDictOffset;
    int PageLengthObjectNumber;
    gs_offset_t PageLengthOffset;
    void *next;
} pdfimage_page;

typedef struct PCLm_temp_file_s {
    char file_name[gp_file_name_sizeof];
    gp_file *file;
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
    int NumPages;
    gs_offset_t RootOffset;
    gs_offset_t PagesOffset;
    gs_offset_t InfoOffset;
    gs_offset_t xrefOffset;
    pdfimage_page *Pages;
    PCLm_temp_file_t xref_stream;
    PCLm_temp_file_t temp_stream;
    int NextObject;

    int Tumble;
    int Tumble2;

    /* OCR data */
    struct {
        char language[1024];
        int engine;
        void *state;

        /* Number of "file level" objects - i.e. the number of objects
         * required to define the font. */
        int file_objects;
        gs_offset_t file_object_offset[OCR_MAX_FILE_OBJECTS];
        int w;
        int h;
        int y;
        int xres;
        int yres;
        float cur_x;
        float cur_y;
        float cur_size;
        float cur_scale;
        float wordbox[4];
        int *word_chars;
        int word_len;
        int word_max;
        void *data;
        /* Write the font definition. */
        int (*file_init)(struct gx_device_pdf_image_s *dev);
        int (*begin_page)(struct gx_device_pdf_image_s *dev, int w, int h, int bpp);
        void (*line)(struct gx_device_pdf_image_s *dev, void *row);
        int (*end_page)(struct gx_device_pdf_image_s *dev);
    } ocr;
} gx_device_pdf_image;

int pdf_image_open(gx_device *pdev);
int pdf_image_close(gx_device * pdev);
int pdf_image_put_params_downscale(gx_device * dev, gs_param_list * plist);
int pdf_image_put_params_downscale_cmyk(gx_device * dev, gs_param_list * plist);
int pdf_image_put_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist);
int pdf_image_get_params_downscale(gx_device * dev, gs_param_list * plist);
int pdf_image_get_params_downscale_cmyk(gx_device * dev, gs_param_list * plist);
int pdf_image_get_params_downscale_cmyk_ets(gx_device * dev, gs_param_list * plist);
dev_proc_print_page(pdf_image_print_page);

#endif
