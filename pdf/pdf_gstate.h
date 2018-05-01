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

/* Graphics state operations for the PDF interpreter */

#ifndef PDF_GSTATE_OPERATORS
#define PDF_GSTATE_OPERATORS

int pdf_concat(pdf_context *ctx);
int pdf_gsave(pdf_context *ctx);
int pdf_grestore(pdf_context *ctx);
int pdf_setlinewidth(pdf_context *ctx);
int pdf_setlinejoin(pdf_context *ctx);
int pdf_setlinecap(pdf_context *ctx);
int pdf_setflat(pdf_context *ctx);
int pdf_setdash(pdf_context *ctx);
int pdf_setmiterlimit(pdf_context *ctx);

#endif
