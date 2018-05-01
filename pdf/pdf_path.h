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

/* Path operations for the PDF interpreter */

#ifndef PDF_PATH_OPERATORS
#define PDF_PATH_OPERATORS

int pdf_moveto (pdf_context *ctx);
int pdf_lineto (pdf_context *ctx);
int pdf_fill (pdf_context *ctx);
int pdf_eofill(pdf_context *ctx);
int pdf_stroke(pdf_context *ctx);
int pdf_closepath_stroke(pdf_context *ctx);
int pdf_curveto(pdf_context *ctx);
int pdf_v_curveto(pdf_context *ctx);
int pdf_y_curveto(pdf_context *ctx);
int pdf_closepath(pdf_context *ctx);
int pdf_newpath(pdf_context *ctx);
int pdf_b(pdf_context *ctx);
int pdf_b_star(pdf_context *ctx);
int pdf_B(pdf_context *ctx);
int pdf_B_star(pdf_context *ctx);
int pdf_clip(pdf_context *ctx);
int pdf_eoclip(pdf_context *ctx);

#endif
