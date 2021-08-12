/* Copyright (C) 2019-2021 Artifex Software, Inc.
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

/* Page-level operations for the PDF interpreter */

#ifndef PDF_PAGE_OPERATORS
#define PDF_PAGE_OPERATORS

typedef enum pdfi_box_enum_e {
    BOX_NONE = 0,
    MEDIA_BOX = 1,
    CROP_BOX = 2,
    TRIM_BOX = 4,
    ART_BOX = 8,
    BLEED_BOX = 16
}pdfi_box_enum;

typedef struct {
    bool HasTransparency;
    int NumSpots;
    pdfi_box_enum boxes;
    float MediaBox[4];
    float CropBox[4];
    float ArtBox[4];
    float BleedBox[4];
    float TrimBox[4];
    float Rotate;
    float UserUnit;
} pdf_info_t;

int pdfi_page_render(pdf_context *ctx, uint64_t page_num, bool init_graphics);
int pdfi_page_info(pdf_context *ctx, uint64_t page_num, pdf_info_t *info);
int pdfi_page_graphics_begin(pdf_context *ctx);
int pdfi_page_get_dict(pdf_context *ctx, uint64_t page_num, pdf_dict **dict);
int pdfi_page_get_number(pdf_context *ctx, pdf_dict *target_dict, uint64_t *page_num);

#endif
