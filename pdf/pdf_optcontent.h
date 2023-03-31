/* Copyright (C) 2019-2023 Artifex Software, Inc.
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

#ifndef PDF_OPTCONTENT
#define PDF_OPTCONTENT

bool pdfi_oc_is_ocg_visible(pdf_context *ctx, pdf_dict *ocdict);

int pdfi_oc_init(pdf_context *ctx);
int pdfi_oc_free(pdf_context *ctx);
bool pdfi_oc_is_off(pdf_context *ctx);
int pdfi_op_MP(pdf_context *ctx);
int pdfi_op_DP(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_op_BMC(pdf_context *ctx);
int pdfi_op_BDC(pdf_context *ctx, pdf_dict *stream_dict, pdf_dict *page_dict);
int pdfi_op_EMC(pdf_context *ctx);

#endif
