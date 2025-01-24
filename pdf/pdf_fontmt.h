/* Copyright (C) 2020-2025 Artifex Software, Inc.
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
#ifdef UFST_BRIDGE
int pdfi_alloc_mt_font(pdf_context *ctx, pdf_string *fco_path, int32_t index, pdf_font_microtype **font);
int pdfi_copy_microtype_font(pdf_context *ctx, pdf_font *spdffont, pdf_dict *font_dict, pdf_font **tpdffont);
int pdfi_free_font_microtype(pdf_obj *font);
#else /* UFST_BRIDGE */
#define pdfi_alloc_mt_font(ctx, fco_path, index, font) gs_error_undefined
#define pdfi_copy_microtype_font(ctx, spdffont, font_dict, tpdffont) gs_error_undefined
#define pdfi_free_font_microtype(font) gs_error_undefined
#endif /* UFST_BRIDGE */
