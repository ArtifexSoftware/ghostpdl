/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

/* colour operations for the PDF interpreter */

#ifndef PDF_COLOUR_OPERATORS
#define PDF_COLOUR_OPERATORS

#include "gscolor1.h"

int pdfi_setgraystroke(pdf_context *ctx);
int pdfi_setgrayfill(pdf_context *ctx);
int pdfi_setrgbstroke(pdf_context *ctx);
int pdfi_setrgbfill(pdf_context *ctx);
int pdfi_setcmykstroke(pdf_context *ctx);
int pdfi_setcmykfill(pdf_context *ctx);
int pdfi_setstrokecolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_setfillcolor_space(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_setstrokecolor(pdf_context *ctx);
int pdfi_setfillcolor(pdf_context *ctx);
int pdfi_setcolorN(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict, bool is_fill);
int pdfi_ri(pdf_context *ctx);

int pdfi_setcolor_from_array(pdf_context *ctx, pdf_array *array);
int pdfi_gs_setgray(pdf_context *ctx, double d);
int pdfi_gs_setrgbcolor(pdf_context *ctx, double r, double g, double b);

/* For potential use by other types of object (images etc) */
int pdfi_gs_setcolorspace(pdf_context *ctx, gs_color_space *pcs);
int pdfi_setcolorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_create_colorspace(pdf_context *ctx, pdf_obj *space, pdf_dict *stream_dict, pdf_dict *page_dict, gs_color_space **ppcs, bool inline_image);
int pdfi_create_icc_colorspace_from_stream(pdf_context *ctx, pdf_stream *stream, gs_offset_t offset,
                                           unsigned int length, int comps, int *icc_N, gs_color_space **ppcs);

/* Page level spot colour detection and enumeration */
int pdfi_check_ColorSpace_for_spots(pdf_context *ctx, pdf_obj *space, pdf_dict *parent_dict, pdf_dict *page_dict, pdf_dict *spot_dict);

#endif
