/* Copyright (C) 2018-2023 Artifex Software, Inc.
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

/* Path operations for the PDF interpreter */

#ifndef PDF_PATH_OPERATORS
#define PDF_PATH_OPERATORS

int pdfi_moveto (pdf_context *ctx);
int pdfi_lineto (pdf_context *ctx);
int pdfi_fill (pdf_context *ctx);
int pdfi_eofill(pdf_context *ctx);
int pdfi_stroke(pdf_context *ctx);
int pdfi_closepath_stroke(pdf_context *ctx);
int pdfi_curveto(pdf_context *ctx);
int pdfi_v_curveto(pdf_context *ctx);
int pdfi_y_curveto(pdf_context *ctx);
int pdfi_closepath(pdf_context *ctx);
int pdfi_newpath(pdf_context *ctx);
int pdfi_b(pdf_context *ctx);
int pdfi_b_star(pdf_context *ctx);
int pdfi_B(pdf_context *ctx);
int pdfi_B_star(pdf_context *ctx);
int pdfi_clip(pdf_context *ctx);
int pdfi_eoclip(pdf_context *ctx);
int pdfi_rectpath(pdf_context *ctx);

#endif
